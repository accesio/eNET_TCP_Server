#include "BRD_.h"
#include "../utilities.h"   // For stuff<>, etc.

// =================== TReadOnlyConfig<T> ===================

// (1) Constructor: DId + offset
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds dId, __u8 offset)
    : TDataItem<ReadOnlyConfigParams<T>>(dId, {})
{
    this->params.offset = offset;
}

// (2) Constructor: DId + raw bytes
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds dId, const TBytes &FromBytes)
    : TDataItem<ReadOnlyConfigParams<T>>(dId, FromBytes)
{
    // By default offset=0. Subclasses can override (like TBRD_DeviceID) if needed
    this->params.offset = 0;
    Debug("TReadOnlyConfig(DataItemIds, TBytes) constructor");
}

// (3) Constructor: just TBytes
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(const TBytes &FromBytes)
    : TDataItem<ReadOnlyConfigParams<T>>(DataItemIds::INVALID, FromBytes)
{
    this->params.offset = 0;
    Debug("TReadOnlyConfig(TBytes) constructor, set offset=0");
}

template <typename T>
TReadOnlyConfig<T>::~TReadOnlyConfig() {}

// Default .Go() → reads hardware
template <typename T>
TReadOnlyConfig<T> &TReadOnlyConfig<T>::Go()
{
    T raw = static_cast<T>(in(this->params.offset));
    this->params.config = raw;
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<T>(this->params.config));
    return *this;
}

// Default .calcPayload() → returns the config
template <typename T>
TBytes TReadOnlyConfig<T>::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<T>(bytes, this->params.config);
    return bytes;
}

// Default .AsString() → uses DIdDict
template <typename T>
std::string TReadOnlyConfig<T>::AsString(bool bAsReply)
{
    auto it = DIdDict.find(this->DId);
    if (it == DIdDict.end())
    {
        return "Unknown TReadOnlyConfig DId=" + std::to_string((int)this->DId);
    }
    if (bAsReply)
    {
        return it->second.desc + " → " + to_hex<T>(this->params.config);
    }
    else
    {
        return it->second.desc;
    }
}

// Explicit instantiations to avoid linker issues if needed:
template class TReadOnlyConfig<__u8>;
template class TReadOnlyConfig<__u16>;
template class TReadOnlyConfig<__u32>;

// =================== TBRD_FpgaId ===================
TBRD_FpgaId::TBRD_FpgaId()
    : TReadOnlyConfig<__u32>(DataItemIds::BRD_FpgaID, ofsFpgaID)
{
    Debug("TBRD_FpgaId() default constructor with offset=ofsFpgaID");
}

TBRD_FpgaId::TBRD_FpgaId(const TBytes &FromBytes)
    : TReadOnlyConfig<__u32>(FromBytes)
{
    this->DId = DataItemIds::BRD_FpgaID;
    this->params.offset = ofsFpgaID;
}

TBRD_FpgaId::TBRD_FpgaId(DataItemIds DId, const TBytes &FromBytes)
    : TReadOnlyConfig<__u32>(DId, FromBytes)
{
    this->params.offset = ofsFpgaID;
}

// Override AsString, Go, calcPayload as needed

std::string TBRD_FpgaId::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_FpgaId() → " + to_hex<__u32>(this->params.config);
    }
    else {
        return "BRD_FpgaId() ? " + to_hex<__u32>(this->params.config);
    }
}

TBRD_FpgaId &TBRD_FpgaId::Go()
{
    this->params.config = static_cast<__u32>(in(this->params.offset));
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u32>(this->params.config));
    return *this;
}

TBytes TBRD_FpgaId::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<__u32>(bytes, this->params.config);
    return bytes;
}

// =================== TBRD_DeviceID ===================
TBRD_DeviceID::TBRD_DeviceID()
    : TReadOnlyConfig<__u16>(DataItemIds::BRD_DeviceID, ofsDeviceID)
{}

TBRD_DeviceID::TBRD_DeviceID(const TBytes &FromBytes)
    : TReadOnlyConfig<__u16>(FromBytes)
{
    this->DId = DataItemIds::BRD_DeviceID;
    this->params.offset = ofsDeviceID;
}

TBRD_DeviceID::TBRD_DeviceID(DataItemIds DId, const TBytes &FromBytes)
    : TReadOnlyConfig<__u16>(DId, FromBytes)
{
    this->params.offset = ofsDeviceID;
}

std::string TBRD_DeviceID::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_DeviceID() → " + to_hex<__u16>(this->params.config);
    }
    else {
        return "BRD_DeviceID() ? " + to_hex<__u16>(this->params.config);
    }
}

TBRD_DeviceID &TBRD_DeviceID::Go()
{
    this->params.config = static_cast<__u16>(in(this->params.offset));
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u16>(this->params.config));
    return *this;
}

TBytes TBRD_DeviceID::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff<__u16>(bytes, this->params.config);
    return bytes;
}

// =================== TBRD_Features ===================
TBRD_Features::TBRD_Features()
    : TReadOnlyConfig<__u8>(DataItemIds::BRD_Features, ofsFeatures)
{}

TBRD_Features::TBRD_Features(const TBytes &FromBytes)
    : TReadOnlyConfig<__u8>(FromBytes)
{
    this->DId = DataItemIds::BRD_Features;
    this->params.offset = ofsFeatures;
}

TBRD_Features::TBRD_Features(DataItemIds DId, const TBytes &FromBytes)
    : TReadOnlyConfig<__u8>(DId, FromBytes)
{
    this->params.offset = ofsFeatures;
}

std::string TBRD_Features::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_Features() → " + to_hex<__u8>(this->params.config);
    }
    else {
        return "BRD_Features() ? " + to_hex<__u8>(this->params.config);
    }
}

TBRD_Features &TBRD_Features::Go()
{
    this->params.config = static_cast<__u8>(in(this->params.offset));
    Log("Offset = " + to_hex<__u8>(this->params.offset)
        + " > " + to_hex<__u8>(this->params.config));
    return *this;
}

TBytes TBRD_Features::calcPayload(bool bAsReply)
{
    TBytes bytes;
    // If your code previously stuffed 4 bytes, do stuff<__u32>.
    // Otherwise, do 1 byte:
    stuff<__u32>(bytes, this->params.config);
    return bytes;
}
