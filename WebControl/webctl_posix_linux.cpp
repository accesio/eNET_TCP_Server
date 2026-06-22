#include "webctl_posix_linux.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "../logging.h"

namespace {

std::atomic<bool> g_stop{false};
std::atomic<int> g_listen_fd{-1};
const WebCtlDeviceOps *g_device = nullptr;

struct FileAssetContext {
    std::string web_root;
    std::vector<unsigned char> file_data;
};

struct ClientArgs { int fd = -1; };

std::string EnvString(const char *name, const char *fallback)
{
    const char *v = std::getenv(name);
    if (v == nullptr || *v == '\0') return fallback ? fallback : "";
    return v;
}

int EnvInt(const char *name, int fallback)
{
    const char *v = std::getenv(name);
    if (v == nullptr || *v == '\0') return fallback;
    char *end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    if (end == v || parsed <= 0 || parsed > 65535) return fallback;
    return static_cast<int>(parsed);
}

bool EnvBool(const char *name, bool fallback)
{
    const char *v = std::getenv(name);
    if (v == nullptr || *v == '\0') return fallback;
    std::string s = v;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s == "1" || s == "true" || s == "yes" || s == "on";
}

void CloseFd(int &fd)
{
    if (fd >= 0) {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        fd = -1;
    }
}

std::string Lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool StartsWithCaseInsensitive(const std::string &s, const std::string &prefix)
{
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(s[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    }
    return true;
}

std::string Trim(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string HeaderValue(const std::string &headers, const char *name)
{
    std::string wanted = Lower(name);
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t next = headers.find("\r\n", pos);
        std::string line = headers.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        pos = (next == std::string::npos) ? headers.size() : next + 2;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = Lower(Trim(line.substr(0, colon)));
        if (key == wanted) return Trim(line.substr(colon + 1));
    }
    return "";
}

int B64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    if (c == '=') return -2;
    return -1;
}

bool DecodeBase64(const std::string &in, std::string &out)
{
    out.clear();
    int val = 0;
    int valb = -8;
    for (char c : in) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int d = B64Value(c);
        if (d == -2) break;
        if (d < 0) return false;
        val = (val << 6) | d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return true;
}

bool BasicAuthOk(const std::string &headers)
{
    if (!EnvBool("AIOENETD_WEB_AUTH_ENABLE", AIOENETD_WEB_AUTH_ENABLE != 0)) return true;
    std::string auth = HeaderValue(headers, "Authorization");
    if (!StartsWithCaseInsensitive(auth, "Basic ")) return false;
    std::string decoded;
    if (!DecodeBase64(Trim(auth.substr(6)), decoded)) return false;
    std::string expected = EnvString("AIOENETD_WEB_AUTH_USERNAME", AIOENETD_WEB_AUTH_USERNAME) + ":" + EnvString("AIOENETD_WEB_AUTH_PASSWORD", AIOENETD_WEB_AUTH_PASSWORD);
    return decoded == expected;
}

const char *MimeTypeForPath(const std::string &path)
{
    std::string ext;
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) ext = Lower(path.substr(dot));
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css; charset=utf-8";
    if (ext == ".js") return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool NormalizeWebPath(const char *request_path, std::string &normalized)
{
    normalized.clear();
    const char *p = (request_path != nullptr && *request_path != '\0') ? request_path : "/";
    for (; *p != '\0' && *p != '?'; ++p) {
        if (*p == '\\') return false;
        normalized.push_back(*p);
    }
    if (normalized.empty()) normalized = "/";
    if (normalized[0] != '/') normalized.insert(normalized.begin(), '/');
    if (normalized == "/") normalized = "/index.html";
    if (normalized.find("..") != std::string::npos) return false;
    return true;
}

int FileAssetLookup(const char *path, WebCtlAsset *out, void *context)
{
    auto *ctx = static_cast<FileAssetContext *>(context);
    if (ctx == nullptr || out == nullptr) return 0;
    std::string normalized;
    if (!NormalizeWebPath(path, normalized)) return 0;
    std::filesystem::path full = std::filesystem::path(ctx->web_root) / std::filesystem::path(normalized.substr(1));
    std::error_code ec;
    if (std::filesystem::is_regular_file(full, ec)) {
        std::ifstream f(full, std::ios::binary);
        if (f.good()) {
            ctx->file_data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            out->path = path;
            out->content_type = MimeTypeForPath(normalized);
            out->data = ctx->file_data.empty() ? reinterpret_cast<const unsigned char *>("") : ctx->file_data.data();
            out->length = ctx->file_data.size();
            out->etag = nullptr;
            return 1;
        }
    }
    unsigned embedded_count = 0;
    const WebCtlAsset *embedded = WebCtl_EmbeddedAssets(&embedded_count);
    return WebCtl_DefaultAssetLookup(path, out, embedded, embedded_count);
}

bool SendAll(int fd, const unsigned char *data, size_t len)
{
    while (len != 0) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
        data += static_cast<size_t>(n);
        len -= static_cast<size_t>(n);
    }
    return true;
}

bool SendText(int fd, const std::string &s)
{
    return SendAll(fd, reinterpret_cast<const unsigned char *>(s.data()), s.size());
}

void SendSimpleError(int fd, unsigned status, const char *reason, const std::string &headers = "")
{
    std::string body = "{\"error\":\"" + std::to_string(status) + "\",\"detail\":\"" + reason + "\"}\r\n";
    std::string response = "HTTP/1.1 " + std::string(WebCtl_StatusText(status)) + "\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) + "\r\n" + headers + "Connection: close\r\n\r\n" + body;
    (void)SendText(fd, response);
}

bool SendWebCtlResponse(int fd, const WebCtlResponse &r)
{
    std::string headers = "HTTP/1.1 " + std::string(r.status_text ? r.status_text : WebCtl_StatusText(r.status)) + "\r\n";
    headers += "Content-Type: ";
    headers += r.content_type ? r.content_type : "application/octet-stream";
    headers += "\r\nContent-Length: " + std::to_string(r.body_length) + "\r\nConnection: close\r\n";
    if (r.headers != nullptr) headers += r.headers;
    headers += "\r\n";
    if (!SendText(fd, headers)) return false;
    if (!r.omit_body && r.body != nullptr && r.body_length != 0) return SendAll(fd, r.body, r.body_length);
    return true;
}

bool ReadHttpRequest(int fd, std::string &method, std::string &path, std::string &headers, std::string &body)
{
    std::string raw;
    raw.reserve(AIOENETD_WEB_REQUEST_HEADER_MAX);
    for (;;) {
        char buf[1024];
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
        raw.append(buf, static_cast<size_t>(n));
        size_t header_end = raw.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            headers = raw.substr(0, header_end + 2);
            size_t line_end = raw.find("\r\n");
            if (line_end == std::string::npos) return false;
            std::string request_line = raw.substr(0, line_end);
            size_t sp1 = request_line.find(' ');
            size_t sp2 = (sp1 == std::string::npos) ? std::string::npos : request_line.find(' ', sp1 + 1);
            if (sp1 == std::string::npos || sp2 == std::string::npos) return false;
            method = request_line.substr(0, sp1);
            path = request_line.substr(sp1 + 1, sp2 - sp1 - 1);
            size_t content_len = 0;
            std::string content_len_text = HeaderValue(headers, "Content-Length");
            if (!content_len_text.empty()) {
                char *end = nullptr;
                unsigned long parsed = std::strtoul(content_len_text.c_str(), &end, 10);
                if (end == content_len_text.c_str()) return false;
                content_len = static_cast<size_t>(parsed);
            }
            if (content_len > AIOENETD_WEB_POST_BODY_MAX) { SendSimpleError(fd, 413u, "request body too large"); return false; }
            size_t body_start = header_end + 4;
            body = raw.substr(body_start);
            while (body.size() < content_len) {
                n = recv(fd, buf, std::min(sizeof(buf), content_len - body.size()), 0);
                if (n < 0) { if (errno == EINTR) continue; return false; }
                if (n == 0) return false;
                body.append(buf, static_cast<size_t>(n));
            }
            if (body.size() > content_len) body.resize(content_len);
            return true;
        }
        if (raw.size() > AIOENETD_WEB_REQUEST_HEADER_MAX) { SendSimpleError(fd, 413u, "request headers too large"); return false; }
    }
}

WebCtlHttpMethod ParseMethod(const std::string &method)
{
    if (method == "GET") return WEBCTL_HTTP_GET;
    if (method == "HEAD") return WEBCTL_HTTP_HEAD;
    if (method == "POST") return WEBCTL_HTTP_POST;
    return WEBCTL_HTTP_OTHER;
}

void *ClientThread(void *arg)
{
    std::unique_ptr<ClientArgs> client(static_cast<ClientArgs *>(arg));
    int fd = client ? client->fd : -1;
    if (fd < 0) return nullptr;
    timeval tv{}; tv.tv_sec = 5; tv.tv_usec = 0;
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    std::string method_text, path, headers, body;
    if (!ReadHttpRequest(fd, method_text, path, headers, body)) { CloseFd(fd); return nullptr; }
    WebCtlHttpMethod method = ParseMethod(method_text);
    if (method == WEBCTL_HTTP_OTHER) { SendSimpleError(fd, 405u, "method not allowed"); CloseFd(fd); return nullptr; }
    if (WebCtl_RouteRequiresAuth(method, path.c_str()) && !BasicAuthOk(headers)) {
        std::string hdr = "WWW-Authenticate: Basic realm=\"" + EnvString("AIOENETD_WEB_AUTH_REALM", AIOENETD_WEB_AUTH_REALM) + "\"\r\n";
        SendSimpleError(fd, 401u, "authentication required", hdr);
        CloseFd(fd);
        return nullptr;
    }
    FileAssetContext asset_context;
    asset_context.web_root = EnvString("AIOENETD_WEB_ROOT", AIOENETD_WEB_ROOT);
    unsigned embedded_count = 0;
    const WebCtlAsset *embedded = WebCtl_EmbeddedAssets(&embedded_count);
    WebCtlAssetProvider provider{};
    provider.assets = embedded;
    provider.asset_count = embedded_count;
    provider.lookup = FileAssetLookup;
    provider.context = &asset_context;
    WebCtlRequest request{};
    request.method = method;
    request.path = path.c_str();
    request.body = body.c_str();
    request.body_length = body.size();
    request.authenticated = true;
    WebCtlResponse response{};
    WebCtlWork work{};
    std::vector<char> scratch(AIOENETD_WEB_RESPONSE_BUFFER_SIZE);
    if (WebCtl_HandleRequestWithWork(&request, &response, g_device, &provider, scratch.data(), scratch.size(), &work) != 0) SendSimpleError(fd, 500u, "web request failed");
    else (void)SendWebCtlResponse(fd, response);
    CloseFd(fd);
    return nullptr;
}

void AcceptLoop(int listen_fd)
{
    while (!g_stop.load()) {
        fd_set readfds; FD_ZERO(&readfds); FD_SET(listen_fd, &readfds);
        timeval tv{}; tv.tv_sec = 1; tv.tv_usec = 0;
        int ret = select(listen_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) { if (errno == EINTR) continue; if (!g_stop.load()) Error(std::string("WebControl select() failed: ") + std::strerror(errno)); break; }
        if (ret == 0) continue;
        sockaddr_storage addr{}; socklen_t addr_len = sizeof(addr);
        int client_fd = accept(listen_fd, reinterpret_cast<sockaddr *>(&addr), &addr_len);
        if (client_fd < 0) { if (errno == EINTR) continue; if (!g_stop.load()) Error(std::string("WebControl accept() failed: ") + std::strerror(errno)); continue; }
        auto *client = new ClientArgs; client->fd = client_fd;
        pthread_t t{};
        if (pthread_create(&t, nullptr, ClientThread, client) == 0) pthread_detach(t);
        else { delete client; CloseFd(client_fd); }
    }
}

void *ListenerThread(void *)
{
    int port = EnvInt("AIOENETD_WEB_PORT", AIOENETD_WEB_HTTP_PORT);
    int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_fd < 0) { Error(std::string("WebControl socket() failed: ") + std::strerror(errno)); return nullptr; }
    int yes = 1; (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    int no = 0; (void)setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no));
    sockaddr_in6 addr{}; addr.sin6_family = AF_INET6; addr.sin6_addr = in6addr_any; addr.sin6_port = htons(static_cast<uint16_t>(port));
    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) { Error("WebControl bind() on port " + std::to_string(port) + " failed: " + std::strerror(errno)); close(listen_fd); return nullptr; }
    if (listen(listen_fd, 16) != 0) { Error(std::string("WebControl listen() failed: ") + std::strerror(errno)); close(listen_fd); return nullptr; }
    g_listen_fd.store(listen_fd);
    Log("WebControl HTTP listener started on port " + std::to_string(port));
    AcceptLoop(listen_fd);
    int expected_fd = listen_fd;
    if (g_listen_fd.compare_exchange_strong(expected_fd, -1)) close(listen_fd);
    Log("WebControl HTTP listener stopped");
    return nullptr;
}

} // namespace

int WebCtlPosixLinux_Start(pthread_t *thread, const WebCtlDeviceOps *device)
{
    if (thread == nullptr || device == nullptr) return -EINVAL;
    g_device = device;
    g_stop.store(false);
    if (pthread_create(thread, nullptr, ListenerThread, nullptr) != 0) return -errno;
    return 0;
}

void WebCtlPosixLinux_Stop(void)
{
    g_stop.store(true);
    int fd = g_listen_fd.exchange(-1);
    if (fd >= 0) { shutdown(fd, SHUT_RDWR); close(fd); }
}
