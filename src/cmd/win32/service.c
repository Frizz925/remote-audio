#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "app/sink.h"
#include "app/source.h"
#include "lib/config.h"
#include "lib/logger.h"
#include "lib/string.h"
#include "lib/thread.h"

#define SVC_NAME         TEXT("RemoteAudioSvc")
#define SVC_DISPLAY_NAME TEXT("Remote Audio Service")
#define SVC_DESCRIPTION  TEXT("Service to run Remote Audio as background process")

#define SVC_LOG_FILE  "remote-audio-svc.log"
#define SVC_CONF_FILE "remote-audio-svc.ini"

#define SVC_ERROR_SIZE 256

#define SVC_THREAD_TIMEOUT_SECONDS 30

typedef struct {
    ra_config_t *config;
    ra_logger_stream_t *stream;
    DWORD exit_code;
    BOOL running;
} SvcThreadContext;

void WINAPI SvcMain(DWORD, LPSTR *);
void WINAPI SvcCtrlHandler(DWORD);

const char *daemon_usage =
    "Usage: %s [command]\n"
    "Commands:\n"
    "    install - Install service\n"
    "    uninstall - Uninstall service\n"
    "    start - Start service\n"
    "    stop - Stop service\n";

SERVICE_STATUS gSvcStatus;
SERVICE_STATUS_HANDLE gSvcStatusHandle;
HANDLE gSvcStopEvent;

static const char *SvcStrError(DWORD err) {
    char *errmsg, *fmtmsg;
    HANDLE heap = GetProcessHeap();
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

static void SvcReportError(const char *cause, const char *errmsg) {
    HANDLE source = RegisterEventSource(NULL, SVC_NAME);
    if (!source) return;
    char buffer[SVC_ERROR_SIZE];
    sprintf_s(buffer, SVC_ERROR_SIZE, "[%s] %s", cause, errmsg);
    const char *strings[2] = {SVC_NAME, buffer};
    ReportEvent(source, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 2, 0, strings, NULL);
    DeregisterEventSource(source);
}

static void SvcReportLastError(const char *cause) {
    const char *errmsg = SvcStrError(GetLastError());
    SvcReportError(cause, errmsg);
    HeapFree(GetProcessHeap(), 0, (LPVOID)errmsg);
}

static void rerror(const char *cause) {
    char errmsg[SVC_ERROR_SIZE];
    strerror_s(errmsg, SVC_ERROR_SIZE, errno);
    SvcReportError(cause, errmsg);
}

static void win_perror(const char *cause) {
    const char *errmsg = SvcStrError(GetLastError());
    fprintf(stderr, "[%s] %s\n", cause, errmsg);
    MessageBox(NULL, errmsg, SVC_DISPLAY_NAME, MB_OK | MB_ICONERROR);
    HeapFree(GetProcessHeap(), 0, (LPVOID)errmsg);
}

static int SvcUsage(const char *name) {
    fprintf(stderr, daemon_usage, name);
    return EXIT_SUCCESS;
}

static void SvcReportStatus(DWORD state, DWORD exit_code, DWORD wait_hint) {
    static DWORD checkpoint = 1;

    gSvcStatus.dwCurrentState = state;
    gSvcStatus.dwWin32ExitCode = exit_code;
    gSvcStatus.dwWaitHint = wait_hint;
    gSvcStatus.dwControlsAccepted = state != SERVICE_START_PENDING ? SERVICE_ACCEPT_STOP : 0;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
        gSvcStatus.dwCheckPoint = 0;
    else
        gSvcStatus.dwCheckPoint = checkpoint++;

    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}

static int SvcRun(int argc, char **argv) {
    SERVICE_TABLE_ENTRY table[2] = {
        {SVC_NAME, SvcMain},
        {NULL, NULL},
    };
    if (!StartServiceCtrlDispatcher(table)) {
        SvcReportLastError("StartServiceCtrlDispatcher");
        return -1;
    }
    return 0;
}

static void SvcSinkThread(void *arg) {
    SvcThreadContext *ctx = (SvcThreadContext *)arg;
    ra_config_section_t *section = ra_config_get_section(ctx->config, "sink");
    ra_logger_t *logger = ra_logger_create(ctx->stream, "RemoteAudioSink");

    const char *argv[3] = {SVC_NAME, NULL, NULL};
    int argc = 1;
    if (section) {
        argv[1] = ra_config_get_value(section, "device");
        argv[2] = ra_config_get_value(section, "port");
    }
    if (argv[2] != NULL)
        argc = 3;
    else if (argv[1] != NULL)
        argc = 2;

    ctx->exit_code = sink_main(logger, argc, argv);
    ctx->running = FALSE;
    ra_logger_destroy(logger);
    SetEvent(gSvcStopEvent);
}

static void SvcSourceThread(void *arg) {
    SvcThreadContext *ctx = (SvcThreadContext *)arg;
    ra_logger_t *logger = ra_logger_create(ctx->stream, "RemoteAudioSource");

    ra_config_section_t *section = ra_config_get_section(ctx->config, "source");
    if (!section) {
        ra_logger_info(logger, "No source config section found. Not running the source service.");
        goto done;
    }
    const char *host = ra_config_get_value(section, "host");
    if (!host) {
        ra_logger_error(logger, "Host config is missing");
        goto err;
    }
    const char *device = ra_config_get_value(section, "device");
    const char *port = ra_config_get_value(section, "port");

    int argc = 2;
    const char *argv[4] = {SVC_NAME, host, device, port};
    if (port != NULL)
        argc = 4;
    else if (device != NULL)
        argc = 3;

    ctx->exit_code = source_main(logger, argc, argv);
    ctx->running = FALSE;
    goto done;

err:
    ctx->exit_code = -1;

done:
    ra_logger_destroy(logger);
    ctx->running = FALSE;
}

static void SvcInit(DWORD argc, LPSTR *argv) {
    int err, rc = 0;
    char pathname[MAX_PATH], svc_dir[MAX_PATH];
    char s_drive[_MAX_DRIVE], s_dir[_MAX_DIR];

    ra_config_t *cfg = ra_config_create();
    ra_thread_t *t_sink = NULL, *t_source = NULL;
    ra_logger_stream_t *stream = NULL;
    ra_logger_t *logger = NULL;

    SvcThreadContext c_sink = {
        .config = cfg,
        .exit_code = 0,
        .running = FALSE,
    };
    SvcThreadContext c_source = {
        .config = cfg,
        .exit_code = 0,
        .running = FALSE,
    };

    gSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!gSvcStopEvent) {
        SvcReportStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    GetModuleFileName(NULL, pathname, MAX_PATH);
    _splitpath(pathname, s_drive, s_dir, NULL, NULL);
    strcat(svc_dir, s_drive);
    strcat(svc_dir, s_dir);

    sprintf_s(pathname, MAX_PATH, "%s\\%s", svc_dir, SVC_LOG_FILE);
    FILE *f_log = fopen(pathname, "a");
    if (!f_log) {
        rerror(pathname);
        rc = errno;
        goto cleanup;
    }
    stream = ra_logger_stream_create(f_log, f_log);
    logger = ra_logger_create(stream, "RemoteAudioService");
    c_sink.stream = c_source.stream = stream;

    sprintf_s(pathname, MAX_PATH, "%s\\%s", svc_dir, SVC_CONF_FILE);
    FILE *f_cfg = fopen(pathname, "r");
    if (f_cfg) {
        ra_config_read(cfg, f_cfg);
        fclose(f_cfg);
    }

    c_sink.running = TRUE;
    t_sink = ra_thread_start(&SvcSinkThread, &c_sink, &err);
    if (err) {
        ra_logger_error(logger, "Failed to start sink thread: %s", strerror(err));
        c_sink.running = FALSE;
        c_sink.exit_code = err;
        goto cleanup;
    }

    c_source.running = TRUE;
    t_source = ra_thread_start(&SvcSourceThread, &c_source, &err);
    if (err) {
        ra_logger_error(logger, "Failed to start source thread: %s", strerror(err));
        c_source.running = FALSE;
        c_source.exit_code = err;
        goto cleanup;
    }

    SvcReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
    WaitForSingleObject(gSvcStopEvent, INFINITE);

cleanup:
    if (c_sink.running) sink_stop();
    if (c_source.running) source_stop();
    if (t_sink) {
        ra_thread_join_timeout(t_sink, SVC_THREAD_TIMEOUT_SECONDS);
        ra_thread_destroy(t_sink);
        if (c_sink.exit_code != 0) rc = c_sink.exit_code;
    }
    if (t_source) {
        ra_thread_join_timeout(t_source, SVC_THREAD_TIMEOUT_SECONDS);
        ra_thread_destroy(t_source);
        if (c_source.exit_code != 0) rc = c_source.exit_code;
    }
    if (logger) ra_logger_destroy(logger);
    if (stream) ra_logger_stream_destroy(stream);
    SvcReportStatus(SERVICE_STOPPED, rc, 0);
}

static int SvcInstall(const char *dev) {
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

static int SvcUninstall() {
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

static int SvcStart() {
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

static int SvcStop() {
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

void WINAPI SvcCtrlHandler(DWORD ctrl) {
    switch (ctrl) {
    case SERVICE_CONTROL_PAUSE:
    case SERVICE_CONTROL_STOP:
        SvcReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        SetEvent(gSvcStopEvent);
        SvcReportStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
        break;
    }
}

void WINAPI SvcMain(DWORD argc, LPSTR *argv) {
    gSvcStatusHandle = RegisterServiceCtrlHandler(SVC_NAME, SvcCtrlHandler);
    if (!gSvcStatusHandle) {
        SvcReportLastError("RegisterServiceCtrlHandler");
        return;
    }
    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;
    SvcReportStatus(SERVICE_START_PENDING, NO_ERROR, 3000);
    SvcInit(argc, argv);
}

int main(int argc, char **argv) {
    const char *cmd = argv[1];
    if (argc >= 2) {
        const char *arg = argc >= 3 ? argv[2] : NULL;
        if (strequal(cmd, "install"))
            return SvcInstall(arg);
        else if (strequal(cmd, "uninstall"))
            return SvcUninstall();
        else if (strequal(cmd, "start"))
            return SvcStart();
        else if (strequal(cmd, "stop"))
            return SvcStop();
        else if (strequal(cmd, "help"))
            return SvcUsage(argv[0]);
    }
    return SvcRun(argc, argv);
}
