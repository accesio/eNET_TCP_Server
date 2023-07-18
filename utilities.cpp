#include <filesystem>
#include <unistd.h>

#include "logging.h"
#include "utilities.h"


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
