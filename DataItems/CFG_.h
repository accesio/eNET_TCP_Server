#pragma once
#include "../utilities.h"
#include "../config.h"
#include "TDataItem.h"


// 1) Define a struct for TCFG_Hostname if you like, or leave empty
struct HostnameParams {
	char hostname[256]; // Or a known max size
};

class TCFG_Hostname : public TDataItem<HostnameParams>
{
public:
    explicit TCFG_Hostname(DataItemIds dId, const TBytes &buf);

    TCFG_Hostname() = delete;

    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
    virtual TCFG_Hostname &Go() override;
};


struct ConfigFieldParams {
    // If you want to store additional data, you can add fields here.
};

template <typename... Ts>
class TConfigField : public TDataItem<ConfigFieldParams>
{
public:
    // 1) Provide your "pointer / size" constructor
    TConfigField(void *ptr, size_t size)
        : TDataItem<ConfigFieldParams>(DataItemIds::INVALID, {}),
          ptr_(ptr),
          size_(size)
    {
        // Possibly log or debug
        LOG_IT;
    }

    // 2) Provide the (DataItemIds, TBytes) constructor, for when the factory calls it
    TConfigField(DataItemIds DId, TBytes bytes)
        : TDataItem<ConfigFieldParams>(DId, bytes),
          ptr_(nullptr),
          size_(0)
    {
        Trace("constructor(TBytes) for TConfigField received: ", bytes);
        // The templated TDataItem constructor sets this->rawBytes = bytes
    }

    // 3) Variadic constructor
    TConfigField(Ts... arguments)
        : TDataItem<ConfigFieldParams>(DataItemIds::INVALID, {}),
          ptr_(nullptr),
          size_(0)
    {
        LOG_IT;
        // Could do something with arguments...
    }

    // Overridden methods

    virtual TConfigField &Go() override
    {
        LOG_IT;
        auto item = DIdDict.find(this->DId);
        if (item != DIdDict.end())
        {
            Trace(item->second.desc);
            Trace("this->rawBytes = ", this->rawBytes);
            item->second.go(this->rawBytes.data());
        }
        else
        {
            Trace("DId not found in DIdDict for TConfigField");
        }
        return *this;
    }

private:
    void* ptr_;
    size_t size_;
};


#pragma endregion

