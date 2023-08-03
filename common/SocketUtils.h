#ifndef _SOCKET_UTILS_H_
#define _SOCKET_UTILS_H_

// #define FILESYSTEM_SOCKET_PREFIX "/tmp/" 
// #define ANDROID_RESERVED_SOCKET_PREFIX "/dev/socket/"

#include <string>
#include <sys/types.h>

// Network Socket

// Returns fd on success, -1 on fail
int SocketInaddrAnyServer(int port, int type, int listenBacklog = 5);
int SocketNetworkServer(const char* host, int port, int type, int listenBacklog = 5);
int SocketNetworkClient(const char* host, int port, int type);
int SocketNetworkClientTimeout(const char* host, int port, int type,
                               int timeout, int* getaddrinfo_error);
                        
// Unix Domain Socket

// Binds a pre-created socket(AF_LOCAL) 's' to 'name'
// Returns 's' on success, -1 on fail
// Does not call listen()
int SocketLocalServerBind(int s, const char* name);

// Connect to peer named "name" on fd
// Returns same fd or -1 on error.
// fd is not closed on error. that's your job.
int SocketLocalClientConnect(int fd, const char *name, int type);

// Returns fd on success, -1 on fail
int SocketLocalServer(const char* name, int type, int listenBacklog = 5);
int SocketLocalClient(const char* name, int type);

// Loopback socket

// Returns fd on success, -1 on fail
int NetworkLoopbackClient(int port, int type, std::string* error);
int NetworkLoopbackServer(int port, int type, std::string* error, bool prefer_ipv4);

// Return client fd if success, -1 if failed
int Accept(int fd);
// Return fd if success, -1 if failed, -ETIMEDOUT if timeout.
// if timeout_ms <= 0, this function is just same as Accept(int)
int AcceptTimeout(int fd, int timeout_ms);

// Returns the local port the socket is bound to or -1 on error.
int SocketGetLocalPort(int fd);

// Turn on TCP keepalives, and set keepalive parameters for this socket.
//
// A parameter value of zero does not set that parameter.
//
// Typical system defaults are:
//
//     idleTime (in seconds)
//     $ cat /proc/sys/net/ipv4/tcp_keepalive_time
//     7200
//
//     numProbes
//     $ cat /proc/sys/net/ipv4/tcp_keepalive_probes
//     9
//
//     probeInterval (in seconds)
//     $ cat /proc/sys/net/ipv4/tcp_keepalive_intvl
//     75
bool EnableTcpKeepAlives(int sock, unsigned idleTime, unsigned numProbes, unsigned probeInterval);
bool DisableTcpKeepAlives(int sock);

void DisableTcpNagle(int fd);

// The *NoInt wrappers retry on EINTR.
ssize_t RecvNoInt(int fd, void* buf, size_t size, int flags = 0);
ssize_t SendNoInt(int fd, const void* buf, size_t size, int flags = 0);

// The *Full wrappers retry on EINTR and also loop until all data is sent or read.
bool RecvFully(int fd, void* buf, size_t size);
bool SendFully(int fd, const void* buf, size_t size);

bool WaitForRecv(int sock, int timeout_ms);
// Waits up to |timeout_ms| to receive up to |size| bytes of data. |timout_ms| of 0 will
// block forever. Returns the number of bytes received or -1 on error/timeout
ssize_t RecvTimeout(int fd, void* buf, size_t size, int timeout_ms);
// Calls Receive() until exactly |size| bytes have been received or an error occurs.
ssize_t RecvFullTimeout(int fd, void* buf, size_t size, int timeout_ms);

#endif  // _SOCKET_UTILS_H_