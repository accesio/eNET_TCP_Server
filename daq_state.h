#pragma once

#include <cstdint>
#include <string>

// The DAQ state is established during startup.  Runtime device-loss detection is
// intentionally out of scope; callers use this state only to distinguish a
// normally opened FPGA/PCIe interface from degraded, diagnostics-only mode.
enum class DaqStatus : std::uint8_t
{
    Uninitialized = 0,
    Ready = 1,
    DeviceDirectoryMissing = 2,
    NoDeviceFound = 3,
    OpenFailed = 4
};

struct DaqStateSnapshot
{
    DaqStatus status = DaqStatus::Uninitialized;
    int lastErrno = 0;
    std::string devicePath;
    std::string reason;
};

bool DaqReady() noexcept;
DaqStateSnapshot GetDaqStateSnapshot();
const char *DaqStatusName(DaqStatus status) noexcept;
void SetDaqReady(const std::string &devicePath);
void SetDaqUnavailable(DaqStatus status, int errorNumber,
                       const std::string &devicePath,
                       const std::string &reason);
