#include <algorithm>
#include <chrono>
#include <errno.h> // for errno
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdio.h> // for renameat2, perror
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

#include "SYS_.h"

#define PATH_ROOT "/home/pb/eNET_TCP_Server/"

std::string generateBackupFilenameWithBuildTime(std::string base) {
	std::string build_date = __DATE__; // Format: Mmm dd yyyy
	std::string build_time = __TIME__; // Format: hh:mm:ss
	std::tm build_tm = {};
	std::istringstream iss(build_date + " " + build_time);
	iss >> std::get_time(&build_tm, "%b %d %Y %H:%M:%S"); // Parse the date and time
	std::ostringstream oss;
	oss << std::put_time(&build_tm, "%Y-%m-%d_%H-%M-%S");
	std::string timestamp = oss.str();
	std::string filename = base + timestamp + ".backup";
	return filename;
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
void UploadFilesByDataItem(TDataItem& item)
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


TSYS_UploadFileName::TSYS_UploadFileName(DataItemIds DId, TBytes buf) : TDataItem(DId)
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
    return DIdDict.find(this->Id)->second.desc;
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


TSYS_UploadFileData::TSYS_UploadFileData(DataItemIds DId, TBytes buf) : TDataItem(DId)
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
    return DIdDict.find(this->Id)->second.desc;
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
