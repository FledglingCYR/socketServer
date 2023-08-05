#ifndef _UNIQUE_FD_H_
#define _UNIQUE_FD_H_

#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

template <typename Closer>
class unique_fd_impl final {
public:
    unique_fd_impl() {}

    explicit unique_fd_impl(int fd) { reset(fd); }
    ~unique_fd_impl() { reset(); }

    unique_fd_impl(const unique_fd_impl&) = delete;
    unique_fd_impl& operator=(const unique_fd_impl&) = delete;
    unique_fd_impl(unique_fd_impl&& other) noexcept { reset(other.release()); }
    unique_fd_impl& operator=(unique_fd_impl&& s) noexcept {
        int fd = s.fd_;
        s.fd_ = -1;
        reset(fd);
        return *this;
    }

    void reset(int new_value = -1) { 
        if (fd_ != -1) {
            int previous_errno = errno;
            close(fd_);
            errno = previous_errno;
        }

        fd_ = new_value;
    }

    int get() const { return fd_; }

    // unique_fd's operator int is dangerous
    // operator int() const { return get(); }

    bool operator<=(int rhs) const { return get() <= rhs; }
    bool operator>(int rhs) const { return get() > rhs; }
    bool operator>=(int rhs) const { return get() >= rhs; }
    bool operator<(int rhs) const { return get() < rhs; }
    bool operator==(int rhs) const { return get() == rhs; }
    bool operator!=(int rhs) const { return get() != rhs; }
    bool operator==(const unique_fd_impl& rhs) const { return get() == rhs.get(); }
    bool operator!=(const unique_fd_impl& rhs) const { return get() != rhs.get(); }
    bool operator<=(const unique_fd_impl& rhs) const { return get() <= rhs.get(); }
    bool operator>(const unique_fd_impl& rhs) const { return get() > rhs.get(); }
    bool operator>=(const unique_fd_impl& rhs) const { return get() >= rhs.get(); }
    bool operator<(const unique_fd_impl& rhs) const { return get() < rhs.get(); }

    // Catch bogus error checks (i.e.: "!fd" instead of "fd != -1").
    bool operator!() const = delete;

    bool ok() const { return get() >= 0; }

    int release() __attribute__((warn_unused_result)) {
        int ret = fd_;
        fd_ = -1;
        return ret;
    }

private:
    template <typename T = Closer>
    static auto close(int fd) -> decltype(T::Close(fd)) {
        T::Close(fd);
    }

    int fd_ = -1;
};

struct DefaultCloser {
    static void Close(int fd) { ::close(fd); }
};

using unique_fd = unique_fd_impl<DefaultCloser>;

// Inline functions, so that they can be used header-only.
template <typename Closer>
inline bool Pipe(unique_fd_impl<Closer>* read, unique_fd_impl<Closer>* write,
                 int flags = O_CLOEXEC) {
    int pipefd[2];

    if (pipe2(pipefd, flags) != 0) {
        return false;
    }


    read->reset(pipefd[0]);
    write->reset(pipefd[1]);
    return true;
}

template <typename Closer>
inline bool Socketpair(int domain, int type, int protocol, unique_fd_impl<Closer>* left,
                       unique_fd_impl<Closer>* right) {
    int sockfd[2];
    if (socketpair(domain, type, protocol, sockfd) != 0) {
        return false;
    }
    left->reset(sockfd[0]);
    right->reset(sockfd[1]);
    return true;
}

template <typename Closer>
inline bool Socketpair(int type, unique_fd_impl<Closer>* left, unique_fd_impl<Closer>* right) {
    return Socketpair(AF_UNIX, type, 0, left, right);
}

// Using fdopen with unique_fd correctly is more annoying than it should be,
// because fdopen doesn't close the file descriptor received upon failure.
inline FILE* Fdopen(unique_fd&& ufd, const char* mode) {
    int fd = ufd.release();
    FILE* file = fdopen(fd, mode);
    if (!file) {
        close(fd);
    }
    return file;
}

// Using fdopendir with unique_fd correctly is more annoying than it should be,
// because fdopen doesn't close the file descriptor received upon failure.
inline DIR* Fdopendir(unique_fd&& ufd) {
    int fd = ufd.release();
    DIR* dir = fdopendir(fd);
    if (dir == nullptr) {
        close(fd);
    }
    return dir;
}

#endif  // _UNIQUE_FD_H_