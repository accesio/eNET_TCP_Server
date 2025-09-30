#include <fstream>
#include <iostream>
#include <exception>

#include "CFG_.h"
#include "../logging.h"
#include "../utilities.h"
#include "../eNET-AIO16-16F.h"

TCFG_Hostname::TCFG_Hostname(DataItemIds id, const TBytes &buf)
    : TDataItem<HostnameParams>(id, buf)
{
    Debug("TCFG_Hostname ctor: Received ", buf);

    std::memset(this->params.hostname, 0, sizeof(this->params.hostname));

    if (!buf.empty())
    {
        size_t copyLen = std::min(buf.size(), sizeof(this->params.hostname) - 1);
        std::memcpy(this->params.hostname, buf.data(), copyLen);
        // The array is guaranteed null-terminated now
    }

    Debug("Parsed hostname: '" + std::string(this->params.hostname) + "'");
}

TBytes TCFG_Hostname::calcPayload(bool bAsReply)
{
    TBytes out;
    size_t len = strnlen(this->params.hostname, sizeof(this->params.hostname));
    out.insert(out.end(),
               reinterpret_cast<const __u8 *>(this->params.hostname),
               reinterpret_cast<const __u8 *>(this->params.hostname) + len);
    return out;
}

std::string TCFG_Hostname::AsString(bool bAsReply)
{
    return "TCFG_Hostname(\"" + std::string(this->params.hostname) + "\")";
}

TCFG_Hostname &TCFG_Hostname::Go()
{
    const std::string requested = SanitizeHostname(std::string(this->params.hostname));
    Debug("TCFG_Hostname::Go() requested='" + requested + "'");

    if (!IsValidHostname(requested))
    {
        Error("TCFG_Hostname: invalid hostname '" + requested + "'; rejecting.");
        return *this;
    }

    // /etc/hostname (atomic)
    try
    {
        WriteFileAtomic("/etc/hostname", requested + "\n");
    }
    catch (const std::exception &ex)
    {
        Error(std::string("Write /etc/hostname failed: ") + ex.what());
        return *this;
    }

    // update /etc/hosts 127.0.1.1
    try
    {
        UpdateEtcHosts127011(requested);
    }
    catch (const std::exception &ex)
    {
        Warn(std::string("Update /etc/hosts failed: ") + ex.what());
    }

    // TODO: do I *want* the hostname to change immediately? eek?!
    if (sethostname(requested.c_str(), requested.size()) != 0)
    {
        Warn("sethostname failed: " + std::string(std::strerror(errno)));
    }
    else
    {
        Debug("sethostname applied.");
    }
    if (access("/usr/bin/hostnamectl", X_OK) == 0 || access("/bin/hostnamectl", X_OK) == 0)
    {
        int rc = system("hostnamectl set-hostname --static --transient --pretty >/dev/null 2>&1");
        Debug(std::string("hostnamectl rc=") + std::to_string(rc));
    }

    // Avahi nudge
    if (access("/usr/sbin/avahi-daemon", X_OK) == 0 || access("/sbin/avahi-daemon", X_OK) == 0)
    {
        int rc = system("systemctl try-restart avahi-daemon.service >/dev/null 2>&1");
        Debug(std::string("avahi-daemon try-restart rc=") + std::to_string(rc));
    }

    Config.Hostname = requested;
    Debug("Hostname updated to '" + Config.Hostname + "' (persisted + applied).");
    return *this;
}