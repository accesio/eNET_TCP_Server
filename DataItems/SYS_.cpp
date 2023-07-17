
#include <fstream>
#include <filesystem>

#include <algorithm>

#include "SYS_.h"

bool sanitizePath(std::string& path) {
    std::filesystem::path basePath = std::filesystem::absolute("/home/pb/"); // Use absolute instead of canonical

    // Check for null bytes in the path
    if (path.find('\0') != std::string::npos) {
        Error("Error: Path contains null bytes.");
        return false;
    }

    // Convert path to absolute path
    std::filesystem::path p = std::filesystem::absolute(path);
    std::string absPath = p.string();

    // Replace incorrect file separators
    std::replace(absPath.begin(), absPath.end(), '\\', std::filesystem::path::preferred_separator);

    // Check if path is within allowed directory
    if (absPath.compare(0, basePath.string().size(), basePath.string()) != 0) {
        Error("Error: Path is not within allowed directory.");
        return false;
    }

    // Update path with sanitized version
    path = absPath;

    return true;
}

std::string GetTempFileName()
{
    std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
    std::filesystem::path tmpFile = tmpDir / "aioenet_upload_XXXXXX";

    char filenameTemplate[1024];
    strncpy(filenameTemplate, tmpFile.c_str(), tmpFile.string().size()+1);
    int fileDescriptor = mkstemp(filenameTemplate);    // Generate unique filename and create/open it

    if (fileDescriptor == -1) {
        Error( "Failed to create temp file.");
        return "";
    }
    close(fileDescriptor); // Close the file descriptor.

    Debug( "Created temp file: " + std::string(filenameTemplate));
    return filenameTemplate;
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
