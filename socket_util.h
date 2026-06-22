#pragma once

#include <cstddef>
#include <sys/types.h>

// Send exactly byteCount bytes unless an error occurs.  Returns byteCount on
// success and -1 on failure, preserving a useful errno value.
ssize_t SendAll(int socketFd, const void *data, std::size_t byteCount) noexcept;
