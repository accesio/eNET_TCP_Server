
#include <fstream>
#include <filesystem>

#include <algorithm>

#include "SYS_.h"

bool sanitizePath(std::string& path) {
    std::filesystem::path basePath = std::filesystem::canonical("/home/acces/");

    if (path.find('\0') != std::string::npos) {
        Error( "Error: Path contains null bytes." );
        return false;
    }

    try {
        std::filesystem::path p = std::filesystem::canonical(path);
        path = p.string();
    } catch(const std::exception& e) {
        Error( "Error: Failed to resolve path: " + std::string(e.what()));
        return false;
    }

    std::replace(path.begin(), path.end(), '\\', std::filesystem::path::preferred_separator); // Replace incorrect file separators

    if (path.compare(0, basePath.string().size(), basePath.string()) != 0) {
        Error( "Error: Path is not within allowed directory.");
        return false;
    }
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

    Debug( "Created temp file: " + filenameTemplate);
    return filenameTemplate;
}

void UploadFilesByDataItem(TDataItem& item)
{
    static std::ofstream ofs;
    static std::string filename;

    if (item.getDId() == DataItemIds::SYS_UploadFileName) {
        if (ofs.is_open()) {
            ofs.close();
            if (!ofs) {
                throw std::runtime_error("File Upload process failed to close file " + filename);
            }
        }
        auto data = item.Data;
        filename = std::string(data.begin(), data.end());
        ofs.open(filename, std::ios::binary);
        if (!ofs) {
            throw std::runtime_error("File Upload process cannot open file " + filename);
        }
    } else if (item.getDId() == DataItemIds::SYS_UploadFileData) {
        if (!ofs.is_open()) {
            throw std::runtime_error("File Upload process received data item before filename");
        }
        auto data = item.Data;
        if (data.size() == 0) {
            ofs.close();
            if (!ofs) {
                throw std::runtime_error("File Upload process failed to close file " + filename);
            }
        } else {
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
    }
    else
    {
        // zero out .Data and change Message to indicate Error
    }

}

TBytes TSYS_UploadFileName::calcPayload(bool bAsReply)
{
    return Data;//getData();
}

std::string TSYS_UploadFileName::AsString(bool bAsReply)
{
    return "UploadFileName";
}

TSYS_UploadFileName & TSYS_UploadFileName::Go()
{
    try{
        UploadFilesByDataItem(*this);
    }
    catch (std::exception e)
    {
        Error(e.what());
    }
    return *this;
}
