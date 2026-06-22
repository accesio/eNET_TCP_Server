#include "webctl_aioenetd.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <ifaddrs.h>
#include <net/if.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "webctl_posix_linux.h"
#include "../apci.h"
#include "../config.h"
#include "../logging.h"

int AioEnetd_RunSerialized(const char *name, std::function<int(void)> work, unsigned timeout_ms);

namespace {

constexpr unsigned kDioBits = 16u;
constexpr unsigned kDioGroups = 16u;
constexpr uint32_t kDioMask = 0xFFFFu;
constexpr unsigned kSerializedTimeoutMs = 1000u;

void CopyText(char *dst, size_t dst_size, const std::string &src)
{
    if (dst == nullptr || dst_size == 0) return;
    std::snprintf(dst, dst_size, "%s", src.c_str());
}

std::string ReadFirstLine(const std::string &path)
{
    std::ifstream f(path);
    std::string s;
    std::getline(f, s);
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

uint64_t UptimeMilliseconds()
{
    timespec ts{};
#ifdef CLOCK_BOOTTIME
    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0)
#endif
    {
        (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    }
    return (static_cast<uint64_t>(ts.tv_sec) * 1000u) + (static_cast<uint64_t>(ts.tv_nsec) / 1000000u);
}

unsigned AdcResolutionBits()
{
    std::string model = Config.Model;
    std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return (model.find("AIO12") != std::string::npos || model.find("AI12") != std::string::npos) ? 12u : 16u;
}

void FillCapabilities(WebCtlCapabilities *out)
{
    std::memset(out, 0, sizeof(*out));
    out->dio_present = true;
    out->dio_bits = kDioBits;
    out->dio_group_count = kDioGroups;
    out->dio_max_bits_per_group = 1u;
    out->dio_direction_configurable = true;
    out->dio_output_writes = true;
    out->network_present = true;
    out->network_apply = false;
    out->usb_present = false;
    out->firmware_update = false;
    out->https_present = false;
    out->certificate_storage = false;
    out->adc_present = true;
    out->adc_snapshot = false;
    out->adc_streaming = true;
    out->adc_channels = Config.adcChannels != 0u ? Config.adcChannels : 16u;
    out->adc_resolution_bits = AdcResolutionBits();
    out->dac_present = Config.NUM_DACs != 0u;
    out->dac_channels = Config.NUM_DACs;
    out->diagnostics_present = true;
}

void FillDioSnapshotFromRegisters(WebCtlDioSnapshot *out, uint32_t directions, uint32_t outputs, uint32_t inputs)
{
    std::memset(out, 0, sizeof(*out));
    directions &= kDioMask;
    outputs &= kDioMask;
    inputs &= kDioMask;
    out->group_count = kDioGroups;
    out->bits = kDioBits;
    out->input_mask = directions;
    out->output_mask = (~directions) & kDioMask;
    out->physical_state = inputs;
    out->output_latch = outputs;
    for (unsigned i = 0; i < kDioGroups; ++i) {
        uint32_t bit = 1u << i;
        WebCtlDioGroup &g = out->groups[i];
        g.group = i;
        g.first_bit = i;
        g.bit_count = 1u;
        g.input = (directions & bit) != 0u;
        g.pins = (inputs & bit) ? 1u : 0u;
        g.output_latch = (outputs & bit) ? 1u : 0u;
        g.bit_mask = bit;
    }
}

int ReadDioSnapshotLocked(WebCtlDioSnapshot *out)
{
    if (apci < 0) return -ENODEV;
    uint32_t directions = in(ofsDioDirections) & kDioMask;
    uint32_t outputs = in(ofsDioOutputs) & kDioMask;
    uint32_t inputs = in(ofsDioInputs) & kDioMask;
    FillDioSnapshotFromRegisters(out, directions, outputs, inputs);
    return 0;
}

std::string DefaultRouteInterface(std::string *gateway_out)
{
    std::ifstream f("/proc/net/route");
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string iface, destination, gateway, flags;
        if (!(iss >> iface >> destination >> gateway >> flags)) continue;
        if (destination != "00000000") continue;
        unsigned long gw = std::strtoul(gateway.c_str(), nullptr, 16);
        in_addr addr{};
        addr.s_addr = static_cast<in_addr_t>(gw);
        char text[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &addr, text, sizeof(text)) != nullptr && gateway_out != nullptr) *gateway_out = text;
        return iface;
    }
    return "";
}

std::string FirstNonLoopbackInterface()
{
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) return "";
    std::string result;
    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name == nullptr || ifa->ifa_addr == nullptr) continue;
        if ((ifa->ifa_flags & IFF_LOOPBACK) != 0u) continue;
        if ((ifa->ifa_flags & IFF_UP) == 0u) continue;
        if (ifa->ifa_addr->sa_family == AF_INET) { result = ifa->ifa_name; break; }
    }
    freeifaddrs(ifaddr);
    return result;
}

void FillInterfaceAddress(WebCtlNetworkInterface *out, const std::string &iface)
{
    ifaddrs *ifaddr = nullptr;
    if (getifaddrs(&ifaddr) != 0) return;
    for (ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_name == nullptr || ifa->ifa_addr == nullptr) continue;
        if (iface != ifa->ifa_name) continue;
        out->present = true;
        out->active = (ifa->ifa_flags & IFF_UP) != 0u;
        if (ifa->ifa_addr->sa_family == AF_INET) {
            auto *addr = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
            auto *mask = reinterpret_cast<sockaddr_in *>(ifa->ifa_netmask);
            char text[INET_ADDRSTRLEN] = {0};
            if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)) != nullptr) CopyText(out->ipv4, sizeof(out->ipv4), text);
            if (mask != nullptr && inet_ntop(AF_INET, &mask->sin_addr, text, sizeof(text)) != nullptr) CopyText(out->netmask, sizeof(out->netmask), text);
        }
    }
    freeifaddrs(ifaddr);
}

void FillNetwork(WebCtlNetworkSnapshot *out)
{
    std::memset(out, 0, sizeof(*out));
    CopyText(out->policy_mode, sizeof(out->policy_mode), "linux-read-only");
    std::string gateway;
    std::string iface = DefaultRouteInterface(&gateway);
    if (iface.empty()) iface = FirstNonLoopbackInterface();
    if (iface.empty()) iface = "eth0";
    CopyText(out->active_interface, sizeof(out->active_interface), iface);
    WebCtlNetworkInterface &eth = out->ethernet;
    FillInterfaceAddress(&eth, iface);
    eth.present = true;
    eth.dhcp = false;
    if (!gateway.empty()) CopyText(eth.gateway, sizeof(eth.gateway), gateway);
    std::string mac = ReadFirstLine("/sys/class/net/" + iface + "/address");
    if (!mac.empty()) CopyText(eth.mac, sizeof(eth.mac), mac);
    std::string operstate = ReadFirstLine("/sys/class/net/" + iface + "/operstate");
    eth.link = operstate == "up" || operstate == "unknown";
    out->stored.valid = eth.present;
    out->stored.dhcp = eth.dhcp;
    CopyText(out->stored.ipv4, sizeof(out->stored.ipv4), eth.ipv4);
    CopyText(out->stored.netmask, sizeof(out->stored.netmask), eth.netmask);
    CopyText(out->stored.gateway, sizeof(out->stored.gateway), eth.gateway);
    CopyText(out->stored.mac, sizeof(out->stored.mac), eth.mac);
    CopyText(out->extra_json, sizeof(out->extra_json), "    \"apply_supported\": false,\r\n    \"dhcp_detection\": \"not_implemented\"");
}

int GetStatus(WebCtlStatusSnapshot *out, void *)
{
    std::memset(out, 0, sizeof(*out));
    CopyText(out->unit.uid, sizeof(out->unit.uid), Config.SerialNumber.empty() ? "unknown" : Config.SerialNumber);
    CopyText(out->unit.model, sizeof(out->unit.model), Config.Model.empty() ? "eNET-AIO" : Config.Model);
    out->unit.model_code = Config.features;
    CopyText(out->unit.revision, sizeof(out->unit.revision), "unknown");
    CopyText(out->unit.firmware, sizeof(out->unit.firmware), AIOENETD_VERSION);
    out->unit.firmware_major = 0u;
    out->unit.firmware_minor = 8u;
    CopyText(out->unit.comm_mode, sizeof(out->unit.comm_mode), "ethernet");
    out->unit.uptime_ms = UptimeMilliseconds();
    out->unit.uptime_ticks = out->unit.uptime_ms;
    CopyText(out->jumpers.network_mode, sizeof(out->jumpers.network_mode), "linux");
    out->jumpers.dhcp_enabled = false;
    out->jumpers.discovery_enabled = true;
    out->jumpers.console_enabled = true;
    out->jumpers.update_enabled = false;
    out->api.version = WEBCTL_API_VERSION;
    CopyText(out->api.phase, sizeof(out->api.phase), "portable-webcontrol-v1-linux");
    out->api.writes_enabled = true;
    out->api.auth_enabled = true;
    return 0;
}

int GetCapabilities(WebCtlCapabilities *out, void *) { FillCapabilities(out); return 0; }

int GetSystem(WebCtlSystemSnapshot *out, void *)
{
    std::memset(out, 0, sizeof(*out));
    CopyText(out->architecture, sizeof(out->architecture), "sitara-am64xx");
    CopyText(out->mcu, sizeof(out->mcu), "AM64xx");
    CopyText(out->board_family, sizeof(out->board_family), "eNET-AIO/eNET-AI");
    CopyText(out->rtos, sizeof(out->rtos), "Linux");
    CopyText(out->tcpip_stack, sizeof(out->tcpip_stack), "POSIX sockets");
    CopyText(out->crypto_stack, sizeof(out->crypto_stack), "none");
    CopyText(out->filesystem, sizeof(out->filesystem), "/home/acces/www + embedded fallback");
    CopyText(out->compiler, sizeof(out->compiler), std::string("GCC ") + __VERSION__);
    CopyText(out->build_date, sizeof(out->build_date), __DATE__);
    CopyText(out->build_time, sizeof(out->build_time), __TIME__);
    CopyText(out->web_control, sizeof(out->web_control), "WebControl/v1");
    CopyText(out->device_shim, sizeof(out->device_shim), "webctl_aioenetd");
    CopyText(out->transport_shim, sizeof(out->transport_shim), "webctl_posix_linux");
    utsname u{};
    if (uname(&u) == 0) {
        std::string extra = "    \"linux\": { \"sysname\": \"" + std::string(u.sysname) + "\", \"release\": \"" + std::string(u.release) + "\", \"machine\": \"" + std::string(u.machine) + "\" },\r\n";
        extra += "    \"hardware\": { \"fpga_id\": \"" + to_hex<__u32>(Config.FpgaVersionCode) + "\", \"features\": \"" + to_hex<__u8>(Config.features) + "\", \"adc_channels\": " + std::to_string(Config.adcChannels) + ", \"dac_channels\": " + std::to_string(Config.NUM_DACs) + " }";
        CopyText(out->extra_json, sizeof(out->extra_json), extra);
    }
    return 0;
}

int DioGet(WebCtlDioSnapshot *out, void *)
{
    return AioEnetd_RunSerialized("webctl dio_get", [out]() { return ReadDioSnapshotLocked(out); }, kSerializedTimeoutMs);
}

int DioWriteOutputs(const WebCtlDioWriteRequest *request, WebCtlDioWriteResult *result, WebCtlDioSnapshot *out, void *)
{
    if (request == nullptr || result == nullptr || out == nullptr) return -EINVAL;
    return AioEnetd_RunSerialized("webctl dio_write", [request, result, out]() {
        if (apci < 0) return -ENODEV;
        uint32_t directions = in(ofsDioDirections) & kDioMask;
        uint32_t latch = in(ofsDioOutputs) & kDioMask;
        uint32_t prior = in(ofsDioInputs) & kDioMask;
        uint32_t requested_mask = static_cast<uint32_t>(request->mask) & kDioMask;
        uint32_t requested_value = static_cast<uint32_t>(request->value) & kDioMask;
        uint32_t output_capable = (~directions) & kDioMask;
        uint32_t applied_mask = requested_mask & output_capable;
        uint32_t ignored_mask = requested_mask & ~output_capable;
        uint32_t applied_value = requested_value & applied_mask;
        uint32_t new_latch = (latch & ~applied_mask) | applied_value;
        if (::out(ofsDioOutputs, new_latch) != ERR_SUCCESS) return -EIO;
        uint32_t after_inputs = in(ofsDioInputs) & kDioMask;
        uint32_t after_outputs = in(ofsDioOutputs) & kDioMask;
        FillDioSnapshotFromRegisters(out, directions, after_outputs, after_inputs);
        std::memset(result, 0, sizeof(*result));
        result->requested_mask = requested_mask;
        result->requested_value = request->value;
        result->applied_mask = applied_mask;
        result->applied_value = applied_value;
        result->ignored_mask = ignored_mask;
        result->prior_physical_state = prior;
        result->after_physical_state = after_inputs;
        if (ignored_mask != 0u) {
            WebCtlNote &note = result->notes[0];
            CopyText(note.code, sizeof(note.code), "input_groups_ignored");
            CopyText(note.detail, sizeof(note.detail), "selected bits belong to input I/O Groups and were ignored by hardware");
            note.mask = ignored_mask;
            result->note_count = 1u;
        }
        return 0;
    }, kSerializedTimeoutMs);
}

int DioSetDirection(const WebCtlDioDirectionRequest *request, WebCtlDioSnapshot *out, void *)
{
    if (request == nullptr || out == nullptr) return -EINVAL;
    return AioEnetd_RunSerialized("webctl dio_direction", [request, out]() {
        if (apci < 0) return -ENODEV;
        uint32_t directions = in(ofsDioDirections) & kDioMask;
        if (request->has_input_mask) directions = static_cast<uint32_t>(request->input_mask) & kDioMask;
        else if (request->has_output_mask) directions = (~static_cast<uint32_t>(request->output_mask)) & kDioMask;
        else if (request->has_group && request->group < kDioGroups) {
            uint32_t bit = 1u << request->group;
            if (request->group_input) directions |= bit;
            else directions &= ~bit;
        } else return -EINVAL;
        if (::out(ofsDioDirections, directions) != ERR_SUCCESS) return -EIO;
        return ReadDioSnapshotLocked(out);
    }, kSerializedTimeoutMs);
}

int NetworkGet(WebCtlNetworkSnapshot *out, void *) { FillNetwork(out); return 0; }

int HttpsGet(WebCtlHttpsSnapshot *out, void *)
{
    std::memset(out, 0, sizeof(*out));
    out->http_port = AIOENETD_WEB_HTTP_PORT;
    out->https_port = 443u;
    CopyText(out->status, sizeof(out->status), "not_configured");
    return 0;
}

int CertificatesGet(WebCtlCertificateSnapshot *out, void *)
{
    std::memset(out, 0, sizeof(*out));
    CopyText(out->storage, sizeof(out->storage), "not_implemented");
    return 0;
}

WebCtlDeviceOps g_ops = {
    nullptr,
    GetStatus,
    GetCapabilities,
    GetSystem,
    DioGet,
    DioWriteOutputs,
    DioSetDirection,
    NetworkGet,
    nullptr,
    nullptr,
    nullptr,
    HttpsGet,
    CertificatesGet,
    nullptr
};

} // namespace

const WebCtlDeviceOps *WebCtlAioEnetd_DeviceOps(void) { return &g_ops; }
int WebCtlAioEnetd_Start(pthread_t *thread) { return WebCtlPosixLinux_Start(thread, WebCtlAioEnetd_DeviceOps()); }
void WebCtlAioEnetd_Stop(void) { WebCtlPosixLinux_Stop(); }
