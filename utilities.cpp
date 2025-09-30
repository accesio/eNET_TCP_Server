#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "logging.h"
#include "utilities.h"

bool sanitizePath(std::string &path)
{
    std::filesystem::path basePath = std::filesystem::absolute("/home/acces/"); // Use absolute instead of canonical

    // Check for null bytes in the path
    if (path.find('\0') != std::string::npos)
    {
        Error("Error: Path contains null bytes.");
        return false;
    }

    // Convert path to absolute path
    std::filesystem::path p = std::filesystem::absolute(path);
    std::string absPath = p.string();

    // Replace incorrect file separators
    std::replace(absPath.begin(), absPath.end(), '\\', std::filesystem::path::preferred_separator);

    // Check if path is within allowed directory
    if (absPath.compare(0, basePath.string().size(), basePath.string()) != 0)
    {
        Error("Error: Path is not within allowed directory.");
        return false;
    }

    path = absPath;

    return true;
}

std::string GetTempFileName()
{
    std::filesystem::path tmpDir = std::filesystem::temp_directory_path();
    std::filesystem::path tmpFile = tmpDir / "aioenet_upload_XXXXXX";

    char filenameTemplate[1024];
    strncpy(filenameTemplate, tmpFile.c_str(), tmpFile.string().size() + 1);
    int fileDescriptor = mkstemp(filenameTemplate);

    if (fileDescriptor == -1)
    {
        Error("Failed to create temp file.");
        return "";
    }
    close(fileDescriptor);

    Debug("Created temp file: " + std::string(filenameTemplate));
    return filenameTemplate;
}

// A helper function to split a string by a given delimiter (e.g. '-').
std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> tokens;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim))
    {
        if (!token.empty())
        {
            tokens.push_back(token);
        }
    }
    return tokens;
}

std::string GetTempFileNameInDir(const std::string &dir, const std::string &prefix)
{
    std::filesystem::create_directories(dir);
    std::filesystem::path tmpl = std::filesystem::path(dir) / (prefix + ".XXXXXX");

    char filenameTemplate[PATH_MAX];
    std::snprintf(filenameTemplate, sizeof(filenameTemplate), "%s", tmpl.c_str());

    int fd = mkstemp(filenameTemplate);
    if (fd == -1)
    {
        Error(std::string("mkstemp(") + filenameTemplate + ") failed: " + std::strerror(errno));
        return "";
    }
    close(fd);
    Debug(std::string("Created temp file in dir: ") + filenameTemplate);
    return filenameTemplate;
}

// ---------- atomic write (fsync + rename within same FS) ----------
void WriteFileAtomic(const std::string &path, const std::string &content, mode_t mode)
{
    const std::filesystem::path dst(path);
    const std::string dir = dst.parent_path().empty() ? "." : dst.parent_path().string();

    // Create temp in the SAME directory to keep rename() atomic
    std::string tmp = GetTempFileNameInDir(dir, dst.filename().string());
    if (tmp.empty())
    {
        throw std::runtime_error("WriteFileAtomic: could not create temp file");
    }

    int fd = ::open(tmp.c_str(), O_WRONLY | O_TRUNC, mode);
    if (fd < 0)
    {
        std::string msg = std::string("open(") + tmp + "): " + std::strerror(errno);
        unlink(tmp.c_str());
        throw std::runtime_error(msg);
    }

    if (fchmod(fd, mode) != 0)
    {
        std::string msg = std::string("fchmod(") + tmp + "): " + std::strerror(errno);
        close(fd);
        unlink(tmp.c_str());
        throw std::runtime_error(msg);
    }

    ssize_t w = ::write(fd, content.data(), content.size());
    if (w < 0 || static_cast<size_t>(w) != content.size())
    {
        std::string msg = std::string("write(") + tmp + "): " + std::strerror(errno);
        close(fd);
        unlink(tmp.c_str());
        throw std::runtime_error(msg);
    }

    if (fsync(fd) != 0)
    {
        std::string msg = std::string("fsync(") + tmp + "): " + std::strerror(errno);
        close(fd);
        unlink(tmp.c_str());
        throw std::runtime_error(msg);
    }
    close(fd);

    if (rename(tmp.c_str(), path.c_str()) != 0)
    {
        std::string msg = std::string("rename(") + tmp + " → " + path + "): " + std::strerror(errno);
        unlink(tmp.c_str());
        throw std::runtime_error(msg);
    }
}

// ---------- dirs ----------
void EnsureDir(const std::string &path, mode_t mode)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        std::filesystem::create_directories(path, ec);
        if (ec)
        {
            throw std::runtime_error("EnsureDir(" + path + "): " + ec.message());
        }
        chmod(path.c_str(), mode);
    }
}

// ---------- hostname validation/sanitization ----------
static bool IsValidHostnameLabel(const std::string &s)
{
    if (s.empty() || s.size() > 63)
        return false;
    if (s.front() == '-' || s.back() == '-')
        return false;
    for (unsigned char c : s)
    {
        if (!(std::isalnum(c) || c == '-'))
            return false;
    }
    return true;
}

std::string SanitizeHostname(std::string s)
{
    // trim
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch)
                                    { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch)
                         { return !std::isspace(ch); })
                .base(),
            s.end());
    // lowercase
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                   { return std::tolower(c); });
    return s;
}

bool IsValidHostname(const std::string &host)
{
    if (host.empty() || host.size() > 253)
        return false;
    size_t i = 0, n = host.size();
    while (i < n)
    {
        size_t j = host.find('.', i);
        if (j == std::string::npos)
            j = n;
        if (!IsValidHostnameLabel(host.substr(i, j - i)))
            return false;
        i = j + 1;
    }
    return true;
}

// ---------- /etc/hosts updater ----------
void UpdateEtcHosts127011(const std::string &newHost)
{
    std::ifstream in("/etc/hosts");
    std::string out, line;
    bool replaced = false;

    while (std::getline(in, line))
    {
        std::string trimmed = line;
        trimmed.erase(trimmed.begin(),
                      std::find_if(trimmed.begin(), trimmed.end(), [](int ch)
                                   { return !std::isspace(ch); }));
        if (!trimmed.empty() && trimmed[0] != '#')
        {
            size_t sp = trimmed.find_first_of(" \t");
            std::string ip = (sp == std::string::npos) ? trimmed : trimmed.substr(0, sp);
            if (ip == "127.0.1.1")
            {
                out += "127.0.1.1\t" + newHost + "\n";
                replaced = true;
                continue;
            }
        }
        out += line + "\n";
    }
    if (!replaced)
        out += "127.0.1.1\t" + newHost + "\n";

    WriteFileAtomic("/etc/hosts", out);
}