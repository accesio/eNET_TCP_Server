#include "socket_util.h"

#include <cerrno>
#include <cstdint>
#include <sys/socket.h>

ssize_t SendAll(int socketFd, const void *data, std::size_t byteCount) noexcept
{
    const auto *bytes = static_cast<const std::uint8_t *>(data);
    std::size_t sent = 0;

    while (sent < byteCount)
    {
        const ssize_t rc = send(socketFd, bytes + sent, byteCount - sent,
                                MSG_NOSIGNAL);
        if (rc > 0)
        {
            sent += static_cast<std::size_t>(rc);
            continue;
        }
        if (rc < 0 && errno == EINTR)
            continue;
        if (rc == 0)
            errno = EPIPE;
        return -1;
    }

    return static_cast<ssize_t>(sent);
}
