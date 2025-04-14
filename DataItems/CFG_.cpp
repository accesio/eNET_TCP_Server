#include <fstream>
#include <iostream>
#include <exception>

#include "CFG_.h"
#include "../logging.h"
#include "../eNET-AIO16-16F.h"


TCFG_Hostname::TCFG_Hostname(DataItemIds dId, const TBytes &buf)
    : TDataItem<HostnameParams>(dId, buf)
{
    Debug("TCFG_Hostname ctor: Received ", buf);

    // 1) Zero out the hostname array
    std::memset(this->params.hostname, 0, sizeof(this->params.hostname));

    // 2) Copy as many bytes as fit into hostname[] (minus 1 for null terminator)
    if (!buf.empty())
    {
        size_t copyLen = std::min(buf.size(), sizeof(this->params.hostname) - 1);
        std::memcpy(this->params.hostname, buf.data(), copyLen);
        // The array is guaranteed null-terminated now
    }

    // 3) For debugging, convert to std::string
    //    You can't do operator+ on a raw char*, so wrap it:
    Debug("Parsed hostname: '" + std::string(this->params.hostname) + "'");
}

TBytes TCFG_Hostname::calcPayload(bool bAsReply)
{
    TBytes out;
    size_t len = strnlen(this->params.hostname, sizeof(this->params.hostname));
    out.insert(out.end(),
               reinterpret_cast<const __u8*>(this->params.hostname),
               reinterpret_cast<const __u8*>(this->params.hostname) + len);
    return out;
}

std::string TCFG_Hostname::AsString(bool bAsReply)
{
    // For debug/log display:
    return "TCFG_Hostname(\"" + std::string(this->params.hostname) + "\")";
}

TCFG_Hostname &TCFG_Hostname::Go()
{
    Debug("TCFG_Hostname::Go() storing config hostname to: '"
          + std::string(this->params.hostname) + "'");

    // For example: Config.hostname = this->params.hostname;
    // or do something else
    return *this;
}
