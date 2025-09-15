#pragma GCC push_options
#pragma GCC sanitize ("")   // empty disables all -fsanitize flags for this TU
#pragma GCC optimize ("O0")

#include "BRD_.h"
#include "../config.h"
#include "../utilities.h"   // For stuff<>, etc.

// =================== TReadOnlyConfig<T> ===================

// (1) Constructor: DId + offset
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds id, __u8 offset)
    : TDataItem<ReadOnlyConfigParams<T>>(id, {})
{
    this->params.offset = offset;
}

// (2) Constructor: DId + raw bytes
template <typename T>
TReadOnlyConfig<T>::TReadOnlyConfig(DataItemIds id, const TBytes &FromBytes)
    : TDataItem<ReadOnlyConfigParams<T>>(id, FromBytes)
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

TBRD_FpgaId::TBRD_FpgaId(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u32>(id, FromBytes)
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
    this->params.config = in(this->params.offset);
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

TBRD_DeviceID::TBRD_DeviceID(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u16>(id, FromBytes)
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

TBRD_Features::TBRD_Features(DataItemIds id, const TBytes &FromBytes)
    : TReadOnlyConfig<__u8>(id, FromBytes)
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
    Config.features = this->params.config;
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
// ——— TBRD_Model ———————————————————————————————————————————————————————

TDataItemBase &TBRD_Model::Go()
{
    // parse incoming bytes as ASCII
    std::string val(rawBytes.begin(), rawBytes.end());
    Debug("TBRD_Model::Go() received model = '" + val + "'");

    // update in-memory config
    Config.Model = val;

    // persist
    if (!SaveBrdConfig(CONFIG_CURRENT)) {
        Error("TBRD_Model::Go() failed to SaveConfig");
    }

    return *this;
}

TBytes TBRD_Model::calcPayload(bool bAsReply)
{
    // echo back the stored model as ASCII, no trailing NUL
    return TBytes(Config.Model.begin(), Config.Model.end());
}

std::string TBRD_Model::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_Model() → \"" + Config.Model + "\"";
    } else {
        std::string sent(rawBytes.begin(), rawBytes.end());
        return "BRD_Model(\"" + sent + "\")";
    }
}

// ——— TBRD_GetModel ————————————————————————————————————————————————————

TBytes TBRD_GetModel::calcPayload(bool /*bAsReply*/)
{
    // always reply with the current model
    return TBytes(Config.Model.begin(), Config.Model.end());
}

std::string TBRD_GetModel::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_GetModel() → \"" + Config.Model + "\"";
    } else {
        return "BRD_GetModel()";
    }
}

// ——— TBRD_SerialNumber —————————————————————————————————————————————

TDataItemBase &TBRD_SerialNumber::Go()
{
    std::string val(rawBytes.begin(), rawBytes.end());
    Debug("TBRD_SerialNumber::Go() received serial = '" + val + "'");

    Config.SerialNumber = val;

    if (!SaveBrdConfig(CONFIG_CURRENT)) {
        Error("TBRD_SerialNumber::Go() failed to SaveConfig");
    }

    return *this;
}

TBytes TBRD_SerialNumber::calcPayload(bool /*bAsReply*/)
{
    return TBytes(Config.SerialNumber.begin(), Config.SerialNumber.end());
}

std::string TBRD_SerialNumber::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_SerialNumber() → \"" + Config.SerialNumber + "\"";
    } else {
        std::string sent(rawBytes.begin(), rawBytes.end());
        return "BRD_SerialNumber(\"" + sent + "\")";
    }
}

// ——— TBRD_GetSerialNumber ————————————————————————————————————————————

TBytes TBRD_GetSerialNumber::calcPayload(bool /*bAsReply*/)
{
    return TBytes(Config.SerialNumber.begin(), Config.SerialNumber.end());
}

std::string TBRD_GetSerialNumber::AsString(bool bAsReply)
{
    if (bAsReply) {
        return "BRD_GetSerialNumber() → \"" + Config.SerialNumber + "\"";
    } else {
        return "BRD_GetSerialNumber()";
    }
}


// -----------------------------------------
// TBRD_GetNumberOfSubmuxes
// -----------------------------------------
TBRD_GetNumberOfSubmuxes::TBRD_GetNumberOfSubmuxes(DataItemIds id, const TBytes &buf)
  : TDataItem<NumberOfSubmuxParams>(id, buf)
{
    // ignore buf
}
TBRD_GetNumberOfSubmuxes::TBRD_GetNumberOfSubmuxes(DataItemIds id)
  : TDataItem<NumberOfSubmuxParams>(id, {})
{}

TBRD_GetNumberOfSubmuxes &TBRD_GetNumberOfSubmuxes::Go()
{
    this->params.value = Config.numberOfSubmuxes;
    return *this;
}

TBytes TBRD_GetNumberOfSubmuxes::calcPayload(bool /*bAsReply*/)
{
    TBytes out;
    out.push_back(this->params.value);
    return out;
}

std::string TBRD_GetNumberOfSubmuxes::AsString(bool bAsReply)
{
    if (bAsReply)
        return "TBRD_GetNumberOfSubmuxes() → " + std::to_string(this->params.value);
    else
        return "TBRD_GetNumberOfSubmuxes()";
}

// -----------------------------------------
// TBRD_GetSubmuxScale
// -----------------------------------------
TBRD_GetSubmuxScale::TBRD_GetSubmuxScale(DataItemIds id, const TBytes &buf)
  : TDataItem<SubmuxScaleParams>(id, buf)
{
    if (buf.size() == 2) {
        this->params.submuxIndex    = buf[0];
        this->params.gainGroupIndex = buf[1];
    } else if (buf.size() == 4) {
        this->params.value = *(float *)(&buf[0]);
    }
}

TBRD_GetSubmuxScale &TBRD_GetSubmuxScale::Go()
{
    auto s = this->params.submuxIndex;
    auto g = this->params.gainGroupIndex;
    if (s < 4 && g < 4) {
        this->params.value = Config.submuxScaleFactors[s][g];
    } else {
        this->params.value = 0.0f;
        Error("TBRD_GetSubmuxScale: index out of range");
    }
    return *this;
}

TBytes TBRD_GetSubmuxScale::calcPayload(bool bAsReply)
{
    TBytes out;
    if (bAsReply){
        const auto *p = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), p, p + sizeof(float));
    }
    else{
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
    }
    return out;
}

std::string TBRD_GetSubmuxScale::AsString(bool bAsReply)
{
    std::ostringstream ss;
    ss << "TBRD_GetSubmuxScale[submux " << int(this->params.submuxIndex)
       << "][group " << int(this->params.gainGroupIndex) << "]";
    if (bAsReply)
        ss << " → " << this->params.value;
    return ss.str();
}

// -----------------------------------------
// TBRD_GetSubmuxOffset
// -----------------------------------------
TBRD_GetSubmuxOffset::TBRD_GetSubmuxOffset(DataItemIds id, const TBytes &buf)
  : TDataItem<SubmuxOffsetParams>(id, buf)
{
    if (buf.size() == 2) {
        this->params.submuxIndex    = buf[0];
        this->params.gainGroupIndex = buf[1];
    } else if (buf.size() == 4) {
        this->params.value = *(float *)(&buf[0]);
    }
}
TBRD_GetSubmuxOffset &TBRD_GetSubmuxOffset::Go()
{
    auto s = this->params.submuxIndex;
    auto g = this->params.gainGroupIndex;
    if (s < 4 &&
        g < 4)
    {
        this->params.value = Config.submuxOffsets[s][g];
    } else {
        this->params.value = 0.0f;
        Error("TBRD_GetSubmuxOffset: index out of range");
    }
    return *this;
}

TBytes TBRD_GetSubmuxOffset::calcPayload(bool bAsReply)
{
    TBytes out;
    if (bAsReply){
        const auto *p = reinterpret_cast<const __u8*>(&this->params.value);
        out.insert(out.end(), p, p + sizeof(float));
    }
    else
    {
        out.push_back(this->params.submuxIndex);
        out.push_back(this->params.gainGroupIndex);
    }
    return out;
}

std::string TBRD_GetSubmuxOffset::AsString(bool bAsReply)
{
    std::ostringstream ss;
    ss << "TBRD_GetSubmuxOffset[submux " << int(this->params.submuxIndex)
       << "][group " << int(this->params.gainGroupIndex) << "]";
    if (bAsReply)
        ss << " → " << this->params.value;
    return ss.str();
}

#pragma GCC pop_options