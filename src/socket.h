#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET Socket;
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
typedef int Socket;
#endif

int socket_startup();
int socket_cleanup();
Socket socket_accept(Socket sock, struct sockaddr *addr, socklen_t *addrlen);
int socket_close(Socket sock);
int socket_address(char *buf, struct sockaddr_in *addr_in);