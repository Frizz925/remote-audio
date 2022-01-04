#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "app/sink.h"
#include "lib/string.h"
#include "lib/thread.h"

#define SVC_NAME TEXT("RemoteAudioSvc")
#define SVC_DISPLAY_NAME TEXT("Remote Audio Service")
#define SVC_DESCRIPTION TEXT("Service to run Remote Audio sink as background process")
#define SVC_EVENT_ID_ERROR ((DWORD)0x00000001)

#define SVC_ERROR_SIZE 256

typedef struct {
    DWORD argc;
    LPSTR *argv;
    DWORD exit_code;
} service_thread_context_t;

const char *daemon_usage =
    "Usage: %s [command]\n"
    "Commands:\n"
    "    install - Install service\n"
    "    uninstall - Uninstall service\n"
    "    start - Start service\n"
    "    stop - Stop service\n";

SERVICE_STATUS svc_status;
SERVICE_STATUS_HANDLE svc_status_handle;
HANDLE svc_stop_event;

ra_thread_t svc_thread;

static const char *win_strerror() {
    char *errmsg, *fmtmsg;
    HANDLE heap = GetProcessHeap();
    DWORD err = GetLastError();
    DWORD res =
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
                      0,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPSTR)&errmsg,
                      0,
                      NULL);
    fmtmsg = (char *)HeapAlloc(heap, 0, SVC_ERROR_SIZE);
    if (!res) {
        sprintf(fmtmsg, "Unknown error, code %d", err);
    } else {
        sprintf_s(fmtmsg, SVC_ERROR_SIZE, "Error %d: %s", err, errmsg);
        LocalFree(errmsg);
    }
    return fmtmsg;
}

static void win_report_error(const char *cause, const char *errmsg) {
    HANDLE source = RegisterEventSource(NULL, SVC_NAME);
    if (!source) return;
    char buffer[SVC_ERROR_SIZE];
    sprintf_s(buffer, SVC_ERROR_SIZE, "[%s] %s", cause, errmsg);
    const char *strings[2] = {SVC_NAME, buffer};
    ReportEvent(source, EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_ID_ERROR, NULL, 2, 0, strings, NULL);
    DeregisterEventSource(source);
}

static void win_rerror(const char *cause) {
    const char *errmsg = win_strerror();
    win_report_error(cause, errmsg);
    HeapFree(GetProcessHeap(), 0, (LPVOID)errmsg);
}

static void rerror(const char *cause) {
    char errmsg[SVC_ERROR_SIZE];
    strerror_s(errmsg, SVC_ERROR_SIZE, errno);
    win_report_error(cause, errmsg);
}

static void win_perror(const char *cause) {
    const char *errmsg = win_strerror();
    fprintf(stderr, "[%s] %s\n", cause, errmsg);
    MessageBox(NULL, errmsg, SVC_DISPLAY_NAME, MB_OK | MB_ICONERROR);
    HeapFree(GetProcessHeap(), 0, (LPVOID)errmsg);
}

static int display_usage(const char *name) {
    fprintf(stderr, daemon_usage, name);
    return EXIT_SUCCESS;
}

static void service_report_status(DWORD state, DWORD exit_code, DWORD wait_hint) {
    static DWORD checkpoint = 1;

    svc_status.dwCurrentState = state;
    svc_status.dwWin32ExitCode = exit_code;
    svc_status.dwWaitHint = wait_hint;
    svc_status.dwControlsAccepted = state != SERVICE_START_PENDING ? SERVICE_ACCEPT_STOP : 0;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        svc_status.dwCheckPoint = 0;
    else
        svc_status.dwCheckPoint = checkpoint++;

    SetServiceStatus(svc_status_handle, &svc_status);
}

static void service_handler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_STOP:
        service_report_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
        SetEvent(svc_stop_event);
        service_report_status(svc_status.dwCurrentState, NO_ERROR, 0);
        break;
    }
}

void service_thread(void *arg) {
    service_thread_context_t *ctx = (service_thread_context_t *)arg;
    ctx->exit_code = sink_main(ctx->argc, ctx->argv);
}

static void service_init(DWORD argc, LPSTR *argv) {
    svc_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!svc_stop_event) {
        service_report_status(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    int err;
    service_thread_context_t ctx = {
        .argc = argc,
        .argv = argv,
        .exit_code = 0,
    };
    svc_thread = ra_thread_start(&service_thread, &ctx, &err);
    if (err) {
        rerror("thread_start");
        service_report_status(SERVICE_STOPPED, err, 0);
        return;
    }
    service_report_status(SERVICE_RUNNING, NO_ERROR, 0);

    WaitForSingleObject(svc_stop_event, INFINITE);
    sink_stop();

    ra_thread_join_timeout(svc_thread, 30);
    ra_thread_destroy(svc_thread);
    service_report_status(SERVICE_STOPPED, ctx.exit_code, 0);
}

static void service_main(DWORD argc, LPSTR *argv) {
    svc_status_handle = RegisterServiceCtrlHandler(SVC_NAME, service_handler);
    if (!svc_status_handle) {
        win_rerror("RegisterServiceCtrlHandler");
        return;
    }
    svc_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    svc_status.dwServiceSpecificExitCode = 0;
    service_report_status(SERVICE_START_PENDING, NO_ERROR, 3000);
    service_init(argc, argv);
}

static int service_run(int argc, char **argv) {
    SERVICE_TABLE_ENTRY table[2] = {
        {SVC_NAME, service_main},
        {NULL, NULL},
    };
    if (!StartServiceCtrlDispatcher(table)) {
        win_rerror("StartServiceCtrlDispatcher");
        return -1;
    }
    return 0;
}

static int service_install(const char *dev) {
    char filename[MAX_PATH];
    GetModuleFileName(NULL, filename, MAX_PATH);

    char pathname[512];
    if (dev != NULL)
        sprintf(pathname, "\"%s\" %s", filename, dev);
    else
        sprintf(pathname, "\"%s\"", filename);

    SC_HANDLE manager = OpenSCManager(NULL, NULL, GENERIC_WRITE);
    if (!manager) {
        win_perror("OpenSCManager");
        return -1;
    }

    SC_HANDLE service = CreateService(manager,
                                      SVC_NAME,
                                      SVC_DISPLAY_NAME,
                                      0,
                                      SERVICE_WIN32_OWN_PROCESS,
                                      SERVICE_AUTO_START,
                                      SERVICE_ERROR_NORMAL,
                                      pathname,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
    if (!service) {
        win_perror("CreateService");
        CloseServiceHandle(manager);
        return -1;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return 0;
}

static int service_uninstall() {
    SC_HANDLE manager = OpenSCManager(NULL, NULL, GENERIC_WRITE);
    if (!manager) {
        win_perror("OpenSCManager");
        return -1;
    }

    SC_HANDLE service = OpenService(manager, SVC_NAME, DELETE | SERVICE_STOP);
    if (!service) {
        win_perror("CreateService");
        CloseServiceHandle(manager);
        return -1;
    }

    int res = 0;
    if (!DeleteService(service)) {
        win_perror("DeleteService");
        res = -1;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return res;
}

static int service_start() {
    SC_HANDLE manager = OpenSCManager(NULL, NULL, GENERIC_READ);
    if (!manager) {
        win_perror("OpenSCManager");
        return -1;
    }

    SC_HANDLE service = OpenService(manager, SVC_NAME, SERVICE_START);
    if (!service) {
        win_perror("CreateService");
        CloseServiceHandle(manager);
        return -1;
    }

    BOOL res = StartService(service, 0, NULL);
    if (!res) win_perror("StartService");

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return res ? 0 : -1;
}

static int service_stop() {
    SC_HANDLE manager = OpenSCManager(NULL, NULL, GENERIC_READ);
    if (!manager) {
        win_perror("OpenSCManager");
        return -1;
    }

    SC_HANDLE service = OpenService(manager, SVC_NAME, SERVICE_STOP);
    if (!service) {
        win_perror("CreateService");
        CloseServiceHandle(manager);
        return -1;
    }

    SERVICE_STATUS_PROCESS ssp;
    BOOL res = ControlService(service, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
    if (!res) win_perror("ControlService");

    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return res ? 0 : -1;
}

int main(int argc, char **argv) {
    const char *cmd = argv[1];
    if (argc >= 2) {
        const char *arg = argc >= 3 ? argv[2] : NULL;
        if (strequal(cmd, "install"))
            return service_install(arg);
        else if (strequal(cmd, "uninstall"))
            return service_uninstall();
        else if (strequal(cmd, "start"))
            return service_start();
        else if (strequal(cmd, "stop"))
            return service_stop();
        else if (strequal(cmd, "help"))
            return display_usage(argv[0]);
    }
    return service_run(argc, argv);
}
