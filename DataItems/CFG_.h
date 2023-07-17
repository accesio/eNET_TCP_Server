#pragma once
#include "../utilities.h"
#include "../config.h"
#include "TDataItem.h"

class TCFG_Hostname : public TDataItem
{
public:
	explicit TCFG_Hostname(TBytes buf);
	TCFG_Hostname() = delete;//:  TDataItem(CFG_Hostname){}
	virtual TBytes calcPayload(bool bAsReply=false);
	virtual std::string AsString(bool bAsReply = false);
	virtual TCFG_Hostname &Go();
protected:
	std::string hostname;
};

#pragma region "class TConfigField" mezzanine

#pragma region "class TConfigField" mezzanine declaration
template <typename... Ts>
class TConfigField : public TDataItem
{
public:
	TConfigField(void *ptr, size_t size) : ptr_(ptr), size_(size){};
	TConfigField(DataItemIds DId, TBytes bytes);
	TConfigField(Ts ... arguments);
	virtual TBytes calcPayload(bool bAsReply=false);
	virtual TConfigField &Go();
	virtual std::string AsString(bool bAsReply = false);

private:
    void* ptr_;
    size_t size_;
};
// // Use
// ConfigField port_field(&config.port, sizeof(config.port));
// ConfigField address_field(&config.address, sizeof(config.address));
// int port = port_field.get<int>();
// port_field.set<int>(8080);
// std::string address = address_field.get<std::string>();
// address_field.set<std::string>("127.0.0.1");


template <typename... Ts>
TConfigField<Ts...> :: TConfigField(DataItemIds DId, TBytes bytes) //: TDataItem(bytes)
{
	Trace("constructor(TBytes) for TConfigField received: ", bytes);
	Data = bytes;
	return;
}

template <typename... Ts>
TConfigField<Ts...> :: TConfigField(Ts ... arguments)
{
	LOG_IT;
	return;
}

template <typename... Ts>
TBytes TConfigField<Ts...> :: calcPayload(bool bAsReply)
{
	LOG_IT;
	TBytes result = Data;
	return result;
}

template <typename... Ts>
TConfigField<Ts...> &TConfigField<Ts...> :: Go()
{
	LOG_IT;
	auto item = DIdDict.find(this->getDId());
	Trace(item->second.desc);
	Trace("this->Data = ", this->Data);
	item->second.go(this->Data.data());
	return *this;
}

template <typename... Ts>
std::string TConfigField<Ts...> :: AsString(bool bAsReply)
{
	LOG_IT;
	return DIdDict.find(this->Id)->second.desc;

}

#pragma endregion

