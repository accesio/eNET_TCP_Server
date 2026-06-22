#include "daq_state.h"

#include <atomic>
#include <mutex>

namespace
{
std::atomic<DaqStatus> gStatus{DaqStatus::Uninitialized};
std::mutex gDetailsMutex;
int gLastErrno = 0;
std::string gDevicePath;
std::string gReason{"DAQ device has not been initialized"};
} // namespace

bool DaqReady() noexcept
{
    return gStatus.load(std::memory_order_acquire) == DaqStatus::Ready;
}

DaqStateSnapshot GetDaqStateSnapshot()
{
    std::lock_guard<std::mutex> lock(gDetailsMutex);
    return DaqStateSnapshot{gStatus.load(std::memory_order_acquire),
                            gLastErrno, gDevicePath, gReason};
}

const char *DaqStatusName(DaqStatus status) noexcept
{
    switch (status)
    {
    case DaqStatus::Uninitialized: return "Uninitialized";
    case DaqStatus::Ready: return "Ready";
    case DaqStatus::DeviceDirectoryMissing: return "DeviceDirectoryMissing";
    case DaqStatus::NoDeviceFound: return "NoDeviceFound";
    case DaqStatus::OpenFailed: return "OpenFailed";
    }
    return "Unknown";
}

void SetDaqReady(const std::string &devicePath)
{
    std::lock_guard<std::mutex> lock(gDetailsMutex);
    gLastErrno = 0;
    gDevicePath = devicePath;
    gReason.clear();
    gStatus.store(DaqStatus::Ready, std::memory_order_release);
}

void SetDaqUnavailable(DaqStatus status, int errorNumber,
                       const std::string &devicePath,
                       const std::string &reason)
{
    std::lock_guard<std::mutex> lock(gDetailsMutex);
    gLastErrno = errorNumber;
    gDevicePath = devicePath;
    gReason = reason;
    gStatus.store(status, std::memory_order_release);
}
