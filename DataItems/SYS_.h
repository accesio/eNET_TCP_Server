#pragma once

#include "TDataItem.h"

#include <cstdint>
#include <string>
#include <system_error>

void UploadFilesByDataItem(TDataItemBase &item);
std::string generateBackupFilenameWithBuildTime(std::string base="aioenetd_");
std::error_code update_symlink_atomic(const char *target, const char *linkpath);
std::error_code Update(TBytes newfile);
std::error_code Revert();

class TSYS_UploadFileName : public TDataItemBase
{
public:
    explicit TSYS_UploadFileName(TBytes buf) : TSYS_UploadFileName(DataItemIds::SYS_UploadFileName, buf) {};
    explicit TSYS_UploadFileName(DataItemIds id, TBytes buf);

    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
    TSYS_UploadFileName &Go() override;
};

class TSYS_UploadFileData : public TDataItemBase
{
public:
    explicit TSYS_UploadFileData(TBytes buf) : TSYS_UploadFileData(DataItemIds::SYS_UploadFileData, buf) {};
    explicit TSYS_UploadFileData(DataItemIds id, TBytes buf);

    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
    TSYS_UploadFileData &Go() override;
};

// SYS_GetDaqStatus reply (little endian):
//   u8 version, u8 DaqStatus, u16 flags (bit0=ready), u32 errno,
//   u16 pathBytes, u16 reasonBytes, path UTF-8, reason UTF-8.
class TSYS_GetDaqStatus : public TDataItemBase
{
public:
    TSYS_GetDaqStatus(DataItemIds id, const TBytes &fromBytes);
    explicit TSYS_GetDaqStatus(DataItemIds id = DataItemIds::SYS_GetDaqStatus);

    TSYS_GetDaqStatus &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

enum class SysLogSource : __u8
{
    AioEnetdService = 1,
    Kernel = 2,
    System = 3
};

enum SysGetLogRequestFlags : __u16
{
    SYS_LOG_INCLUDE_PREVIOUS_BOOTS = 0x0001
};

enum SysGetLogResponseFlags : __u16
{
    SYS_LOG_MORE = 0x0001,
    SYS_LOG_EOF = 0x0002,
    SYS_LOG_TRUNCATED_ENTRY = 0x0004,
    SYS_LOG_CURSOR_GAP = 0x0008
};

// SYS_GetLog request V1 (little endian):
//   u8 version(1), u8 source (1=service, 2=kernel, 3=system),
//   u16 flags, u16 maxEntries, u16 maxTextBytes, u16 cursorBytes,
//   cursor UTF-8.  An empty request selects the newest aioenetd.service page
//   with defaults.  Supply the reply cursor to retrieve the next older page.
// Text records within every page are ordered chronologically.
//
// Reply V1:
//   u8 version, u8 source, u16 flags, u16 entryCount, u16 cursorBytes,
//   u32 textBytes, u64 firstRealtimeUsec, u64 lastRealtimeUsec,
//   cursor UTF-8, text UTF-8.
class TSYS_GetLog : public TDataItemBase
{
public:
    TSYS_GetLog(DataItemIds id, const TBytes &fromBytes);

    TSYS_GetLog &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;

private:
    __u8 protocolVersion = 1;
    SysLogSource source = SysLogSource::AioEnetdService;
    __u16 requestFlags = 0;
    __u16 maxEntries = 0;
    __u16 maxTextBytes = 0;
    std::string cursor;
};

// SYS_GetBuildInfo reply V1 (little endian):
//   u8 version, 3 reserved bytes, u32 major/minor/patch/build,
//   u16 lengths for version/git hash/git describe/build UTC, then the strings.
class TSYS_GetBuildInfo : public TDataItemBase
{
public:
    TSYS_GetBuildInfo(DataItemIds id, const TBytes &fromBytes);
    explicit TSYS_GetBuildInfo(DataItemIds id = DataItemIds::SYS_GetBuildInfo);

    TSYS_GetBuildInfo &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

inline constexpr unsigned SYS_TEMPERATURE_COUNT = 2;
inline constexpr std::int32_t SYS_TEMPERATURE_UNAVAILABLE = INT32_MIN;

// SYS_ReadTemperatures reply (little endian):
//   s32 tempMillic[SYS_TEMPERATURE_COUNT]
// Static order for this eNET image:
//   [0] /sys/class/thermal/thermal_zone0/temp, documented as main0-thermal
//   [1] /sys/class/thermal/thermal_zone1/temp, documented as main1-thermal
// Unavailable/read-error value: SYS_TEMPERATURE_UNAVAILABLE.
class TSYS_ReadTemperatures : public TDataItemBase
{
public:
    TSYS_ReadTemperatures(DataItemIds id, const TBytes &fromBytes);
    explicit TSYS_ReadTemperatures(DataItemIds id = DataItemIds::SYS_ReadTemperatures);

    TSYS_ReadTemperatures &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

#pragma pack(push, 1)
struct SYS_ErrorParams
{
    __u32 Stage = 0;
    __u32 ErrorCode = ERR_SUCCESS;
    __u32 Info = 0;
};

struct SYS_ItemErrorParams
{
    __u16 ItemIndex = 0;
    __u16 DId = 0;
    __u32 ErrorCode = ERR_SUCCESS;
    __u32 Info = 0;
};
#pragma pack(pop)

class TSYS_Error : public TDataItem<SYS_ErrorParams>
{
public:
    TSYS_Error(DataItemIds id, const TBytes &FromBytes);
    TSYS_Error(__u32 stage, TError errorCode, __u32 info);

    TSYS_Error &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};

class TSYS_ItemError : public TDataItem<SYS_ItemErrorParams>
{
public:
    TSYS_ItemError(DataItemIds id, const TBytes &FromBytes);
    TSYS_ItemError(__u16 itemIndex, DataItemIds originalDid, TError errorCode, __u32 info);

    TSYS_ItemError &Go() override;
    TBytes calcPayload(bool bAsReply = false) override;
    std::string AsString(bool bAsReply = false) override;
};
