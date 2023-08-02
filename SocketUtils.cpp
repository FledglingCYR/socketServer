#include "SocketUtils.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/un.h>
#include <stddef.h>

#define LISTEN_BACKLOG 5

/* Only the bottom bits are really the socket type; there are flags too. */
#define SOCK_TYPE_MASK 0xf

static void set_error(std::string* error) {
    if (error) {
        *error = strerror(errno);
    }
}

static int toggle_O_NONBLOCK(int s) {
    int flags = fcntl(s, F_GETFL);
    if (flags == -1 || fcntl(s, F_SETFL, flags ^ O_NONBLOCK) == -1) {
        close(s);
        return -1;
    }
    return s;
}

int SocketNetworkServer(const char* host, int port, int type, int listenBacklog) {
    struct sockaddr_in addr;
    int s, n;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);

    s = socket(AF_INET, type, 0);
    if (s < 0) return -1;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    if (type == SOCK_STREAM) {
        int ret;

        if (listenBacklog <= 0) listenBacklog = LISTEN_BACKLOG;
        ret = listen(s, listenBacklog);

        if (ret < 0) {
            close(s);
            return -1; 
        }
    }

    return s;
}

int SocketInaddrAnyServer(int port, int type, int listenBacklog) {
    struct sockaddr_in6 addr;
    int s, n;

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any;

    s = socket(AF_INET6, type, 0);
    if (s < 0) return -1;

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }

    if (type == SOCK_STREAM) {
        int ret;

        if (listenBacklog <= 0) listenBacklog = LISTEN_BACKLOG;
        ret = listen(s, listenBacklog);

        if (ret < 0) {
            close(s);
            return -1; 
        }
    }

    return s;
}

int SocketNetworkClient(const char* host, int port, int type) {
    int getaddrinfo_error;
    return SocketNetworkClientTimeout(host, port, type, 0, &getaddrinfo_error);
}

// Connect to the given host and port.
// 'timeout' is in seconds (0 for no timeout).
// Returns a file descriptor or -1 on error.
// On error, check *getaddrinfo_error (for use with gai_strerror) first;
// if that's 0, use errno instead.
int SocketNetworkClientTimeout(const char* host, int port, int type, int timeout,
                               int* getaddrinfo_error) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = type;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo* addrs;
    *getaddrinfo_error = getaddrinfo(host, port_str, &hints, &addrs);
    if (*getaddrinfo_error != 0) {
        return -1;
    }

    int result = -1;
    for (struct addrinfo* addr = addrs; addr != NULL; addr = addr->ai_next) {
        // The Mac doesn't have SOCK_NONBLOCK.
        int s = socket(addr->ai_family, type, addr->ai_protocol);
        if (s == -1 || toggle_O_NONBLOCK(s) == -1) break;

        int rc = connect(s, addr->ai_addr, addr->ai_addrlen);
        if (rc == 0) {
            result = toggle_O_NONBLOCK(s);
            break;
        } else if (rc == -1 && errno != EINPROGRESS) {
            close(s);
            continue;
        }

        fd_set r_set;
        FD_ZERO(&r_set);
        FD_SET(s, &r_set);
        fd_set w_set = r_set;

        struct timeval ts;
        ts.tv_sec = timeout;
        ts.tv_usec = 0;
        if ((rc = select(s + 1, &r_set, &w_set, NULL, (timeout != 0) ? &ts : NULL)) == -1) {
            close(s);
            break;
        }
        if (rc == 0) {  // we had a timeout
            errno = ETIMEDOUT;
            close(s);
            break;
        }

        int error = 0;
        socklen_t len = sizeof(error);
        if (FD_ISSET(s, &r_set) || FD_ISSET(s, &w_set)) {
            if (getsockopt(s, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                close(s);
                break;
            }
        } else {
            close(s);
            break;
        }

        if (error) {  // check if we had a socket error
            // TODO: Update the timeout.
            errno = error;
            close(s);
            continue;
        }

        result = toggle_O_NONBLOCK(s);
        break;
    }

    freeaddrinfo(addrs);
    return result;
}

int SocketLocalServerBind(int s, const char* name) {
    struct sockaddr_un addr;
    socklen_t alen;
    int n;

    memset(&addr, 0, sizeof (addr));
    const size_t namelen = strlen(name);
    if ((namelen + 1) > sizeof(addr.sun_path)) {
        return -1;
    }
    strcpy(addr.sun_path, name);
    addr.sun_family = AF_LOCAL;
    alen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;

    /* basically: if this is a filesystem path, unlink first */
    /*ignore ENOENT*/
    unlink(addr.sun_path);

    n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

    if (bind(s, (struct sockaddr *) &addr, alen) < 0) {
        return -1;
    }

    return s;
}

int SocketLocalServer(const char *name, int type, int listenBacklog) {
    int err;
    int s;
    
    s = socket(AF_LOCAL, type, 0);
    if (s < 0) return -1;

    err = SocketLocalServerBind(s, name);

    if (err < 0) {
        close(s);
        return -1;
    }

#ifdef __ANDROID__
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "chmod 666 %s", name);
    system(cmd);
    // chmod(name, S_IRWXU | S_IRWXG | S_IRWXO);
#endif

    if ((type & SOCK_TYPE_MASK) == SOCK_STREAM) {
        int ret;

        if (listenBacklog <= 0) listenBacklog = LISTEN_BACKLOG;
        ret = listen(s, listenBacklog);

        if (ret < 0) {
            close(s);
            return -1;
        }
    }

    return s;
}

int SocketLocalClientConnect(int fd, const char *name, int type) {
    struct sockaddr_un addr;
    socklen_t alen;

    memset(&addr, 0, sizeof (addr));
    const size_t namelen = strlen(name);
    if ((namelen + 1) > sizeof(addr.sun_path)) {
        return -1;
    }
    strcpy(addr.sun_path, name);
    addr.sun_family = AF_LOCAL;
    alen = namelen + offsetof(struct sockaddr_un, sun_path) + 1;

    if (connect(fd, (struct sockaddr *) &addr, alen) < 0) {
        return -1;
    }

    return fd;
}

int SocketLocalClient(const char *name, int type) {
    int s;

    s = socket(AF_LOCAL, type, 0);
    if(s < 0) return -1;

    if (0 > SocketLocalClientConnect(s, name, type)) {
        close(s);
        return -1;
    }

    return s;
}

static sockaddr* loopback_addr4(sockaddr_storage* addr, socklen_t* addrlen, int port) {
    struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(addr);
    *addrlen = sizeof(*addr4);

    addr4->sin_family = AF_INET;
    addr4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr4->sin_port = htons(port);
    return reinterpret_cast<sockaddr*>(addr);
}

static sockaddr* loopback_addr6(sockaddr_storage* addr, socklen_t* addrlen, int port) {
    struct sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(addr);
    *addrlen = sizeof(*addr6);

    addr6->sin6_family = AF_INET6;
    addr6->sin6_addr = in6addr_loopback;
    addr6->sin6_port = htons(port);
    return reinterpret_cast<sockaddr*>(addr);
}

static int _network_loopback_client(bool ipv6, int port, int type, std::string* error) {
    int s = socket(ipv6 ? AF_INET6 : AF_INET, type, 0);
    if (s == -1) {
        set_error(error);
        return -1;
    }

    struct sockaddr_storage addr_storage = {};
    socklen_t addrlen = sizeof(addr_storage);
    sockaddr* addr = (ipv6 ? loopback_addr6 : loopback_addr4)(&addr_storage, &addrlen, 0);

    if (bind(s, addr, addrlen) != 0) {
        set_error(error);
        goto failed;
    }

    addr = (ipv6 ? loopback_addr6 : loopback_addr4)(&addr_storage, &addrlen, port);

    if (connect(s, addr, addrlen) != 0) {
        set_error(error);
        goto failed;
    }

    return s;

failed:
    close(s);
    return -1;
}

int NetworkLoopbackClient(int port, int type, std::string* error) {
    // Try IPv4 first, use IPv6 as a fallback.
    int rc = _network_loopback_client(false, port, type, error);
    if (rc == -1) {
        return _network_loopback_client(true, port, type, error);
    }
    return rc;
}

static int _network_loopback_server(bool ipv6, int port, int type, std::string* error) {
    int s = socket(ipv6 ? AF_INET6 : AF_INET, type, 0);
    if (s == -1) {
        set_error(error);
        return -1;
    }

    int n = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &n, sizeof(n));

    struct sockaddr_storage addr_storage = {};
    socklen_t addrlen = sizeof(addr_storage);
    sockaddr* addr = (ipv6 ? loopback_addr6 : loopback_addr4)(&addr_storage, &addrlen, port);

    if (bind(s, addr, addrlen) != 0) {
        set_error(error);
        goto failed;
    }

    if (type == SOCK_STREAM || type == SOCK_SEQPACKET) {
        if (listen(s, SOMAXCONN) != 0) {
            set_error(error);
            goto failed;
        }
    }

    return s;

failed:
    close(s);
    return -1;
}

int NetworkLoopbackServer(int port, int type, std::string* error, bool prefer_ipv4) {
    int rc = -1;
    if (prefer_ipv4) {
        rc = _network_loopback_server(false, port, type, error);
    }

    // Only attempt to listen on IPv6 if IPv4 is unavailable or prefer_ipv4 is false
    // We don't want to start an IPv6 server if there's already an IPv4 one running.
    if (rc == -1 && (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT || !prefer_ipv4)) {
        return _network_loopback_server(true, port, type, error);
    }
    return rc;
}

int Accept(int fd) {
    return accept(fd, nullptr, nullptr);
}

int AcceptTimeout(int fd, int timeout_ms) {
    if (timeout_ms > 0) {
        int ret;
        fd_set rset;
        struct timeval timeout = {timeout_ms / 1000, timeout_ms % 1000};
        FD_ZERO(&rset);
        FD_SET(fd, &rset);
        ret = select(fd + 1, &rset, NULL, NULL, &timeout);
        if (ret == -1) return -1;
        if (ret == 0) return -ETIMEDOUT;
    }

    return Accept(fd);
}

int SocketGetLocalPort(int fd) {
    sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);

    if (getsockname(fd, reinterpret_cast<sockaddr*>(&addr_storage), &addr_len) < 0) {
        printf("SocketGetLocalPort: getsockname failed: %s", strerror(errno));
        return -1;
    }

    if (!(addr_storage.ss_family == AF_INET || addr_storage.ss_family == AF_INET6)) {
        printf("SocketGetLocalPort: unknown address family received: %d", addr_storage.ss_family);
        errno = ECONNABORTED;
        return -1;
    }

    return ntohs(reinterpret_cast<sockaddr_in*>(&addr_storage)->sin_port);
}

bool EnableTcpKeepAlives(int sock, unsigned idleTime, unsigned numProbes, unsigned probeInterval) {
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable))) {
        return false;
    }

    if (idleTime != 0) {
        if (setsockopt(sock, SOL_TCP, TCP_KEEPIDLE, &idleTime, sizeof(idleTime))) {
            return false;
        }
    }
    if (numProbes != 0) {
        if (setsockopt(sock, SOL_TCP, TCP_KEEPCNT, &numProbes, sizeof(numProbes))) {
            return false;
        }
    }
    if (probeInterval != 0) {
        if (setsockopt(sock, SOL_TCP, TCP_KEEPINTVL, &probeInterval,
                sizeof(probeInterval))) {
            return false;
        }
    }
    return true;
}

bool DisableTcpKeepAlives(int sock) {
    int enable = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable))) {
        return false;
    }
    return true;
}

inline void DisableTcpNagle(int fd) {
    int off = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &off, sizeof(off));
}

ssize_t RecvNoInt(int fd, void* buf, size_t size, int flags) {
     return TEMP_FAILURE_RETRY(recv(fd, buf, size, flags));
}

ssize_t SendNoInt(int fd, const void* buf, size_t size, int flags) {
    return TEMP_FAILURE_RETRY(send(fd, buf, size, flags));
}

bool RecvFully(int fd, void* buf, size_t size) {
    char* b = static_cast<char*>(buf);
    ssize_t r;

    while (size) {
        r = TEMP_FAILURE_RETRY(recv(fd, b, size, 0));
        if (r <= 0) {
            return false;
        }
        b += r;
        size -= r;
    }

    return true;
}

bool SendFully(int fd, const void* buf, size_t size) {
    const char* b = static_cast<const char*>(buf);
    ssize_t r;

    while (size) {
        r =  TEMP_FAILURE_RETRY(send(fd, b, size, 0));
        if (r <= 0) {
            return false;
        }
        b += r;
        size -= r;
    }

    return true;
}

bool WaitForRecv(int sock, int timeout_ms) {
    // In our usage |timeout_ms| <= 0 means block forever, so just return true immediately and let
    // the subsequent recv() do the blocking.
    if (timeout_ms <= 0) {
        return true;
    }

    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);

    timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    int result = TEMP_FAILURE_RETRY(select(sock + 1, &read_set, nullptr, nullptr, &timeout));
    if (result == -1) {
        printf("select error\n");
    }
    return result == 1;
}

ssize_t RecvTimeout(int fd, void* buf, size_t size, int timeout_ms) {
    if (!WaitForRecv(fd, timeout_ms)) {
        return -1;
    }

    return TEMP_FAILURE_RETRY(recv(fd, buf, size, 0));
}

ssize_t RecvFullTimeout(int fd, void* buf, size_t size, int timeout_ms) {
    size_t total = 0;

    while (total < size) {
        ssize_t bytes = RecvTimeout(fd, static_cast<char*>(buf) + total, size - total, timeout_ms);

        // Returns 0 only when the peer has disconnected because our requested length is not 0. So
        // we return immediately to avoid dead loop here.
        if (bytes <= 0) {
            if (total == 0) {
                return -1;
            }
            break;
        }
        total += bytes;
    }

    return total;
}