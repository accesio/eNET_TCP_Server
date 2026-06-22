#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <errno.h> // for errno
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdio.h> // for renameat2, perror
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

#include "SYS_.h"
#include "../TError.h"
#include "../build_info.h"
#include "../daq_state.h"
#include "../journal_log.h"
#include "../utilities.h"

#define PATH_ROOT "/home/acces/eNET_TCP_Server/"

std::string generateBackupFilenameWithBuildTime(std::string base) {
	const auto now = std::chrono::system_clock::now();
	const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
	std::tm utc{};
	gmtime_r(&seconds, &utc);
	const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
		now.time_since_epoch()).count() % 1000000;
	std::ostringstream oss;
	oss << base << std::put_time(&utc, "%Y-%m-%d_%H-%M-%S")
		<< '-' << std::setfill('0') << std::setw(6) << micros << ".backup";
	return oss.str();
}

std::error_code update_symlink_atomic(const char* target, const char* linkpath) {

	std::string tmp = generateBackupFilenameWithBuildTime(PATH_ROOT + std::string("temp_link"));
	if (std::filesystem::exists(tmp))
	{
		std::filesystem::remove(tmp);
	}

	Debug("Creating temporary symlink " + tmp + " → " + std::string(target));
	if (symlink(target, tmp.c_str()) == -1)
	{
		Debug("Cannot create temp link " + tmp);
        return std::error_code(errno, std::generic_category());
	}

	Debug("Moving " + tmp + " onto " + std::string(linkpath));
    if (renameat2(AT_FDCWD, tmp.c_str(), AT_FDCWD, linkpath, RENAME_EXCHANGE) == -1)
	{
		Debug("Can't renameat2() the temp link onto the actual link");
        return std::error_code(errno, std::generic_category());
	}
    return std::error_code();
}


// copies the contents of newfile as a new copy of `aioenetd` without ever leaving the system in a non-working state
// That is:
// 1) Creates backup copy of currently running aioenetd
// 2) changes the symlink that runs at boot to point at backup
// 3) writes the new data as aioenetd
// 4) changes the symlink to point at updated aioenetd
std::error_code Update(TBytes newfile) {
	std::error_code ec;
	std::string backupFile = PATH_ROOT + generateBackupFilenameWithBuildTime("aioenetd_");
// 1)
	Debug("Copying aioenetd to " + backupFile);
	std::filesystem::copy(PATH_ROOT + std::string("aioenetd"), backupFile, ec);
	if (ec) return ec;
// 2)
	ec = update_symlink_atomic(backupFile.c_str(), "/opt/aioenet/aioenetd");
	if (ec)	return ec;
// 3)
	Debug("Opening File for writing");
	std::ofstream file(PATH_ROOT + std::string("aioenetd"), std::ios::binary);
	if (!file) return std::error_code(errno, std::generic_category());

	Debug("Writing file contents");
	file.write(reinterpret_cast<const char*>(newfile.data()), newfile.size());
	if (file.fail()){
		file.close();
		return std::error_code(errno, std::generic_category());
	}

	Debug("Closing File");
	file.close();
	if (file.fail()) return std::error_code(errno, std::generic_category());

	Debug("Setting the executable bit");
	if (chmod((PATH_ROOT + std::string("aioenetd")).c_str(), S_IRWXU) == -1) {
		Debug("Failed to set the executable bit");
		return std::error_code(errno, std::generic_category());
	}

	// TODO: Verify the new executable here.  MD5?

// 4)
	ec = update_symlink_atomic((PATH_ROOT + std::string("aioenetd")).c_str(), "/opt/aioenet/aioenetd");
	if (ec)	return ec;
	return std::error_code();
}


std::error_code Revert() {
    std::error_code ec;
    std::string backupFile = PATH_ROOT + generateBackupFilenameWithBuildTime("aioenetd_");
	std::string actualFile = PATH_ROOT + std::string("aioenetd");

	if (!std::filesystem::exists(backupFile)) {
        return std::make_error_code(std::errc::no_such_file_or_directory);
    }

    // 1. Rename (move) the backup file to the original file. This will overwrite the original file.
    std::filesystem::rename(backupFile, actualFile, ec);
    if (ec) return ec;

    // 2. Update the symbolic link to point back to the original file
    ec = update_symlink_atomic(actualFile.c_str(), "/opt/aioenet/aioenetd");
    if (ec) return ec;

    return std::error_code();
}



std::ofstream ofs;
std::string filename;
        // Data = {};
void UploadFilesByDataItem(TDataItemBase& item)
{
    if (item.getDId() == DataItemIds::SYS_UploadFileName) {
        Debug("SYS_UploadFileName()");
        if (ofs.is_open()) {
            Debug("File is open");
            ofs.close();
            if (!ofs) {
                Debug("File Upload process failed to close file " + filename);
                throw std::runtime_error("File Upload process failed to close file " + filename);
            }
        }
        auto data = item.Data;
        filename = std::string(data.begin(), data.end());
        Debug("Opening File: "+filename);
        ofs.open(filename, std::ios::binary);
        if (!ofs) {
            Debug("File Upload process cannot open file " + filename);
            throw std::runtime_error("File Upload process cannot open file " + filename);
        }
    } else if (item.getDId() == DataItemIds::SYS_UploadFileData) {
        Debug("SYS_UploadFileData()");
        if (!ofs.is_open()) {
            Debug("File Upload process received data item before filename");
            throw std::runtime_error("File Upload process received data item before filename");
        }
        auto data = item.Data;
        if (item.Data.size() == 0) {
            Debug("Data Size == 0, Closing.");

            ofs.close();
            if (!ofs) {
                Debug("File Upload process failed to close file " + filename);
                throw std::runtime_error("File Upload process failed to close file " + filename);
            }

            // if filename is `aioenetd` then send the completed file to std::error_code Update(TBytes newfile)
            // apparently `vector vec(fd)` is sufficient to load the entire file "fd" into memory vector, can I
            // pass the file descriptor (prior to closing it, I imagine....?) as the TBytes parameter to Update()?
            //
            if (filename == "/home/pb/eNET_TCP_Server/aioenetd.new")
            {
                Debug("UPDATING");
                std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
                if (!ifs) {
                    Debug("Cannot open " + filename + " for reading.");
                    throw std::runtime_error("Cannot open " + filename + " for reading.");
                }

                std::ifstream::pos_type pos = ifs.tellg();
                TBytes newfile(pos);

                ifs.seekg(0, std::ios::beg);
                ifs.read(reinterpret_cast<char*>(newfile.data()), pos);
                ifs.close();


                std::error_code ec = Update(newfile);
                if (ec) {
                    Error("Error in Update: " + ec.message());
                        Debug("REVERTING ");
                    Revert();
                    // handle error...
                }
            }
        } else {
            Debug("Writing data: "+ std::to_string(data.size())+" bytes.");
            ofs.write(reinterpret_cast<const char *>(data.data()), data.size());
            if (!ofs) {
                throw std::runtime_error("File Upload process failed to write to file " + filename);
            }
        }
    }
}


TSYS_UploadFileName::TSYS_UploadFileName(DataItemIds id, TBytes buf)
     : TDataItemBase(id)
{
    std::string str = std::string(buf.begin(), buf.end());
    if (sanitizePath(str))
    {
        // copy sanitized path into this.Data
        std::vector<__u8> vec(str.begin(), str.end());
        Data = vec;
        Debug("Sanitized filename: "+str+": ",Data);
    }
    else
    {
        Data = {};

        //TODO: zero out .Data and change Message to indicate Error
    }
}

TBytes TSYS_UploadFileName::calcPayload(bool bAsReply)
{
    return Data;//getData();
}

std::string TSYS_UploadFileName::AsString(bool bAsReply)
{
    return DIdDict.find(this->DId)->second.desc;
}

TSYS_UploadFileName & TSYS_UploadFileName::Go()
{
    try{
        Debug("Calling UploadFilesByDataItem()");
        UploadFilesByDataItem(*this);
    }
    catch (const std::exception & e)
    {
        Error(e.what());
    }
    return *this;
}


TSYS_UploadFileData::TSYS_UploadFileData(DataItemIds id, TBytes buf)
    : TDataItemBase(id)
{
    Data = buf;
    //Debug("Buffer received: ", buf);
}

TBytes TSYS_UploadFileData::calcPayload(bool bAsReply)
{
    Debug("CalcPayload() called for FileData");
    //Data = {};
    return Data;
}

std::string TSYS_UploadFileData::AsString(bool bAsReply)
{
    return DIdDict.find(this->DId)->second.desc;
}

TSYS_UploadFileData & TSYS_UploadFileData::Go()
{
    try{
        //Debug("Calling UploadFilesByDataItem():", Data);
        UploadFilesByDataItem(*this);
    }
    catch (const std::runtime_error & e)
    {
        Error(e.what());
    }
    return *this;
}

// ---------------- TSYS_Error and TSYS_ItemError Utilities ----------------

static inline __u16 ReadU16LE(const TBytes &b, size_t offset)
{
	if (offset + 2 > b.size()) return 0;
	return static_cast<__u16>(static_cast<__u16>(b[offset]) | (static_cast<__u16>(b[offset + 1]) << 8));
}

static inline __u32 ReadU32LE(const TBytes &b, size_t offset)
{
	if (offset + 4 > b.size()) return 0;
	return static_cast<__u32>(b[offset]) | (static_cast<__u32>(b[offset + 1]) << 8) | (static_cast<__u32>(b[offset + 2]) << 16) | (static_cast<__u32>(b[offset + 3]) << 24);
}

static inline void WriteU16LE(TBytes &out, __u16 v)
{
	out.push_back(static_cast<__u8>(v & 0xFF));
	out.push_back(static_cast<__u8>((v >> 8) & 0xFF));
}

static inline void WriteU32LE(TBytes &out, __u32 v)
{
	out.push_back(static_cast<__u8>(v & 0xFF));
	out.push_back(static_cast<__u8>((v >> 8) & 0xFF));
	out.push_back(static_cast<__u8>((v >> 16) & 0xFF));
	out.push_back(static_cast<__u8>((v >> 24) & 0xFF));
}

static inline void WriteU64LE(TBytes &out, std::uint64_t v)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		out.push_back(static_cast<__u8>((v >> shift) & 0xFFU));
}

static inline __u32 ErrorIndexFromWire(__u32 code)
{
	__u32 errorCount = 0;
	while (err_msg[errorCount]) ++errorCount;

	// SYS_Error and SYS_ItemError carry positive indexes on the wire.  Accept
	// wrapped negative TError values too so local diagnostic construction remains
	// readable while the legacy unsigned TError typedef is still in use.
	return code < errorCount ? code : -code;
}

static inline const char *SafeErrMsgFromCode(__u32 code)
{
	const __u32 idx = ErrorIndexFromWire(code);
	for (__u32 i = 0; err_msg[i]; i++)
	{
		if (i == idx) return err_msg[i];
	}
	return "Unknown error";
}

// ---------------- SYS_GetDaqStatus ----------------

TSYS_GetDaqStatus::TSYS_GetDaqStatus(DataItemIds id, const TBytes &fromBytes)
    : TDataItemBase(id)
{
    if (!fromBytes.empty())
    {
        this->resultCode = ERR_DId_BAD_PARAM;
        this->errorInfo = static_cast<__u32>(fromBytes.size());
    }
}

TSYS_GetDaqStatus::TSYS_GetDaqStatus(DataItemIds id)
    : TDataItemBase(id)
{
}

TSYS_GetDaqStatus &TSYS_GetDaqStatus::Go()
{
    if (this->resultCode != ERR_SUCCESS)
        return *this;

    const DaqStateSnapshot state = GetDaqStateSnapshot();
    const std::string path = state.devicePath.substr(0, 4096);
    const std::string reason = state.reason.substr(0, 4096);

    TBytes out;
    out.reserve(12 + path.size() + reason.size());
    out.push_back(1);
    out.push_back(static_cast<__u8>(state.status));
    WriteU16LE(out, DaqReady() ? 1U : 0U);
    WriteU32LE(out, static_cast<__u32>(state.lastErrno));
    WriteU16LE(out, static_cast<__u16>(path.size()));
    WriteU16LE(out, static_cast<__u16>(reason.size()));
    out.insert(out.end(), path.begin(), path.end());
    out.insert(out.end(), reason.begin(), reason.end());
    this->Data = std::move(out);
    this->resultCode = ERR_SUCCESS;
    return *this;
}

TBytes TSYS_GetDaqStatus::calcPayload(bool /*bAsReply*/)
{
    return this->Data;
}

std::string TSYS_GetDaqStatus::AsString(bool /*bAsReply*/)
{
    const DaqStateSnapshot state = GetDaqStateSnapshot();
    return "SYS_GetDaqStatus(status=" + std::string(DaqStatusName(state.status)) +
           ", errno=" + std::to_string(state.lastErrno) + ")";
}

// ---------------- SYS_GetLog ----------------

TSYS_GetLog::TSYS_GetLog(DataItemIds id, const TBytes &fromBytes)
    : TDataItemBase(id)
{
    if (fromBytes.empty())
        return;

    if (fromBytes.size() < 10)
    {
        this->resultCode = ERR_DId_BAD_PARAM;
        this->errorInfo = static_cast<__u32>(fromBytes.size());
        return;
    }

    this->protocolVersion = fromBytes[0];
    this->source = static_cast<SysLogSource>(fromBytes[1]);
    this->requestFlags = ReadU16LE(fromBytes, 2);
    this->maxEntries = ReadU16LE(fromBytes, 4);
    this->maxTextBytes = ReadU16LE(fromBytes, 6);
    const __u16 cursorBytes = ReadU16LE(fromBytes, 8);

    const bool validSource = this->source == SysLogSource::AioEnetdService ||
                             this->source == SysLogSource::Kernel ||
                             this->source == SysLogSource::System;
    const bool validFlags = (this->requestFlags & ~SYS_LOG_INCLUDE_PREVIOUS_BOOTS) == 0;
    const bool validLength = cursorBytes <= 1024 &&
                             fromBytes.size() == 10U + cursorBytes;
    if (this->protocolVersion != 1 || !validSource || !validFlags || !validLength)
    {
        this->resultCode = ERR_DId_BAD_PARAM;
        this->errorInfo = EINVAL;
        return;
    }

    this->cursor.assign(fromBytes.begin() + 10, fromBytes.end());
    if (this->cursor.find('\0') != std::string::npos)
    {
        this->resultCode = ERR_DId_BAD_PARAM;
        this->errorInfo = EINVAL;
    }
}

TSYS_GetLog &TSYS_GetLog::Go()
{
    if (this->resultCode != ERR_SUCCESS)
        return *this;

    JournalLog::Source journalSource = JournalLog::Source::AioEnetdService;
    if (this->source == SysLogSource::Kernel)
        journalSource = JournalLog::Source::Kernel;
    else if (this->source == SysLogSource::System)
        journalSource = JournalLog::Source::System;
    const bool includePreviousBoots =
        (this->requestFlags & SYS_LOG_INCLUDE_PREVIOUS_BOOTS) != 0;
    JournalLog::Page page = JournalLog::ReadPage(
        journalSource, includePreviousBoots, this->cursor,
        this->maxEntries, this->maxTextBytes);

    if (page.errorNumber != 0)
    {
        this->resultCode = ERR_LOG_UNAVAILABLE;
        this->errorInfo = static_cast<__u32>(page.errorNumber);
        this->Data.clear();
        return *this;
    }
    if (page.nextCursor.size() > 1024U ||
        page.text.size() > static_cast<std::size_t>(MaxDataItemPayload - 28U - 1024U))
    {
        this->resultCode = ERR_DATAITEM_TOO_LARGE;
        this->errorInfo = static_cast<__u32>(page.nextCursor.size() + page.text.size());
        this->Data.clear();
        return *this;
    }

    __u16 responseFlags = 0;
    if (page.more) responseFlags |= SYS_LOG_MORE;
    if (page.eof) responseFlags |= SYS_LOG_EOF;
    if (page.truncatedEntry) responseFlags |= SYS_LOG_TRUNCATED_ENTRY;
    if (page.cursorGap) responseFlags |= SYS_LOG_CURSOR_GAP;

    TBytes out;
    out.reserve(28 + page.nextCursor.size() + page.text.size());
    out.push_back(1);
    out.push_back(static_cast<__u8>(this->source));
    WriteU16LE(out, responseFlags);
    WriteU16LE(out, page.entryCount);
    WriteU16LE(out, static_cast<__u16>(page.nextCursor.size()));
    WriteU32LE(out, static_cast<__u32>(page.text.size()));
    WriteU64LE(out, page.firstRealtimeUsec);
    WriteU64LE(out, page.lastRealtimeUsec);
    out.insert(out.end(), page.nextCursor.begin(), page.nextCursor.end());
    out.insert(out.end(), page.text.begin(), page.text.end());
    this->Data = std::move(out);
    this->resultCode = ERR_SUCCESS;
    return *this;
}

TBytes TSYS_GetLog::calcPayload(bool /*bAsReply*/)
{
    return this->Data;
}

std::string TSYS_GetLog::AsString(bool /*bAsReply*/)
{
    return "SYS_GetLog(source=" +
           std::to_string(static_cast<unsigned>(this->source)) +
           ", cursorBytes=" + std::to_string(this->cursor.size()) + ")";
}

// ---------------- SYS_GetBuildInfo ----------------

TSYS_GetBuildInfo::TSYS_GetBuildInfo(DataItemIds id, const TBytes &fromBytes)
    : TDataItemBase(id)
{
    if (!fromBytes.empty())
    {
        this->resultCode = ERR_DId_BAD_PARAM;
        this->errorInfo = static_cast<__u32>(fromBytes.size());
    }
}

TSYS_GetBuildInfo::TSYS_GetBuildInfo(DataItemIds id)
    : TDataItemBase(id)
{
}

TSYS_GetBuildInfo &TSYS_GetBuildInfo::Go()
{
    if (this->resultCode != ERR_SUCCESS)
        return *this;

    const std::string version = BuildInfo::Version;
    const std::string hash = BuildInfo::GitHash;
    const std::string describe = BuildInfo::GitDescribe;
    const std::string utc = BuildInfo::BuildUtc;
    if (version.size() > 0xFFFFU || hash.size() > 0xFFFFU ||
        describe.size() > 0xFFFFU || utc.size() > 0xFFFFU)
    {
        this->resultCode = ERR_DATAITEM_TOO_LARGE;
        return *this;
    }

    TBytes out;
    out.reserve(28U + version.size() + hash.size() + describe.size() + utc.size());
    out.push_back(1U);
    out.insert(out.end(), 3U, 0U);
    WriteU32LE(out, BuildInfo::Major);
    WriteU32LE(out, BuildInfo::Minor);
    WriteU32LE(out, BuildInfo::Patch);
    WriteU32LE(out, BuildInfo::Build);
    WriteU16LE(out, static_cast<__u16>(version.size()));
    WriteU16LE(out, static_cast<__u16>(hash.size()));
    WriteU16LE(out, static_cast<__u16>(describe.size()));
    WriteU16LE(out, static_cast<__u16>(utc.size()));
    out.insert(out.end(), version.begin(), version.end());
    out.insert(out.end(), hash.begin(), hash.end());
    out.insert(out.end(), describe.begin(), describe.end());
    out.insert(out.end(), utc.begin(), utc.end());
    this->Data = std::move(out);
    this->resultCode = ERR_SUCCESS;
    this->errorInfo = 0;
    return *this;
}

TBytes TSYS_GetBuildInfo::calcPayload(bool /*bAsReply*/)
{
    return this->Data;
}

std::string TSYS_GetBuildInfo::AsString(bool /*bAsReply*/)
{
    return std::string("SYS_GetBuildInfo() -> ") + BuildInfo::Version;
}

// ---------------- TSYS_Error ----------------

TSYS_Error::TSYS_Error(DataItemIds id, const TBytes &FromBytes)
	: TDataItem<SYS_ErrorParams>(id, FromBytes)
{
	this->params.Stage = ReadU32LE(FromBytes, 0);
	this->params.ErrorCode = ReadU32LE(FromBytes, 4);
	this->params.Info = ReadU32LE(FromBytes, 8);
}

TSYS_Error::TSYS_Error(__u32 stage, TError errorCode, __u32 info)
	: TDataItem<SYS_ErrorParams>(DataItemIds::SYS_Error, {})
{
	this->params.Stage = stage;
	this->params.ErrorCode = static_cast<__u32>(errorCode);
	this->params.Info = info;
}

TSYS_Error &TSYS_Error::Go()
{
	this->resultCode = ERR_SUCCESS;
	return *this;
}

TBytes TSYS_Error::calcPayload(bool /*bAsReply*/)
{
	TBytes out;
	out.reserve(12);
	WriteU32LE(out, this->params.Stage);
	WriteU32LE(out, this->params.ErrorCode);
	WriteU32LE(out, this->params.Info);
	return out;
}

std::string TSYS_Error::AsString(bool /*bAsReply*/)
{
	const __u32 idx = ErrorIndexFromWire(this->params.ErrorCode);
	return "SYS_Error(Stage=" + to_hex<__u32>(this->params.Stage) + ", ErrorCode=" + to_hex<__u32>(this->params.ErrorCode) + " (idx=" + std::to_string(idx) + " " + SafeErrMsgFromCode(this->params.ErrorCode) + "), Info=" + to_hex<__u32>(this->params.Info) + ")";
}

// ---------------- TSYS_ItemError ----------------

TSYS_ItemError::TSYS_ItemError(DataItemIds id, const TBytes &FromBytes)
	: TDataItem<SYS_ItemErrorParams>(id, FromBytes)
{
	this->params.ItemIndex = ReadU16LE(FromBytes, 0);
	this->params.DId = ReadU16LE(FromBytes, 2);
	this->params.ErrorCode = ReadU32LE(FromBytes, 4);
	this->params.Info = ReadU32LE(FromBytes, 8);
}

TSYS_ItemError::TSYS_ItemError(__u16 itemIndex, DataItemIds originalDid, TError errorCode, __u32 info)
	: TDataItem<SYS_ItemErrorParams>(DataItemIds::SYS_ItemError, {})
{
	this->params.ItemIndex = itemIndex;
	this->params.DId = static_cast<__u16>(originalDid);
	this->params.ErrorCode = static_cast<__u32>(errorCode);
	this->params.Info = info;
}

TSYS_ItemError &TSYS_ItemError::Go()
{
	this->resultCode = ERR_SUCCESS;
	return *this;
}

TBytes TSYS_ItemError::calcPayload(bool /*bAsReply*/)
{
	TBytes out;
	out.reserve(12);
	WriteU16LE(out, this->params.ItemIndex);
	WriteU16LE(out, this->params.DId);
	WriteU32LE(out, this->params.ErrorCode);
	WriteU32LE(out, this->params.Info);
	return out;
}

std::string TSYS_ItemError::AsString(bool /*bAsReply*/)
{
	const __u32 idx = ErrorIndexFromWire(this->params.ErrorCode);
	return "SYS_ItemError(ItemIndex=" + std::to_string(this->params.ItemIndex) + ", DId=0x" + to_hex<__u16>(this->params.DId) + ", ErrorCode=" + to_hex<__u32>(this->params.ErrorCode) + " (idx=" + std::to_string(idx) + " " + SafeErrMsgFromCode(this->params.ErrorCode) + "), Info=" + to_hex<__u32>(this->params.Info) + ")";
}
