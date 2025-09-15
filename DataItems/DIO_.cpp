#include "DIO_.h"
#include "../apci.h"
#include "../eNET-AIO16-16F.h"
#include "../logging.h"
#include "TDataItem.h"
#include <sstream>
#include <iomanip>
#include "../TError.h"



// --------------------------------------------------------------------------
// TDIO_Configure Implementation
// --------------------------------------------------------------------------
TDIO_Configure::TDIO_Configure(DataItemIds id, const TBytes &data)
    : TDataItem<TDIO_ConfigureParams>(id, data)
{
    if(data.size() >= 2) {
        // Assuming little-endian: data[0]=low byte, data[1]=high byte.
        this->params.value = static_cast<__u16>(static_cast<__u16>(data[0]) | (static_cast<__u16>(data[1]) << 8));
    }
}

TDIO_Configure::TDIO_Configure(DataItemIds id, __u16 value)
    : TDataItem<TDIO_ConfigureParams>(id, {})
{
    this->params.value = value;
}

TDataItemBase &TDIO_Configure::Go() {
    // Write the 16-bit configuration to the DIO direction register.
    // Here, we assume that 'out()' can accept a 32-bit value and we send our 16-bit value.
    out(ofsDioDirections, static_cast<__u32>(this->params.value));
    return *this;
}

TBytes TDIO_Configure::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(static_cast<__u8>(this->params.value & 0xFF));
    bytes.push_back(static_cast<__u8>((this->params.value >> 8) & 0xFF));
    return bytes;
}

std::string TDIO_Configure::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_Configure(value=0x" << std::hex << std::setw(4) << std::setfill('0')
       << this->params.value << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_Input Implementation
// --------------------------------------------------------------------------
TDIO_Input::TDIO_Input(DataItemIds id, const TBytes &data)
    : TDataItem<TDIO_InputParams>(id, data)
{
    // No parameters needed; this command just triggers a read.
}

TDIO_Input::TDIO_Input(DataItemIds id)
    : TDataItem<TDIO_InputParams>(id, {})
{
}

TDataItemBase &TDIO_Input::Go() {
    // Read the digital inputs from the input register.
    __u32 regValue = in(ofsDioInputs);
    this->params.value = static_cast<__u16>(regValue & 0xFFFF);
    return *this;
}

TBytes TDIO_Input::calcPayload(bool bAsReply) {
    TBytes bytes;
    if(bAsReply) {
        bytes.push_back(static_cast<__u8>(this->params.value & 0xFF));
        bytes.push_back(static_cast<__u8>((this->params.value >> 8) & 0xFF));
    }
    return bytes;
}

std::string TDIO_Input::AsString(bool bAsReply) {
    std::stringstream ss;
    ss << "TDIO_Input(";
    if(bAsReply) {
        ss << "value=0x" << std::hex << std::setw(4) << std::setfill('0') << this->params.value;
    } else {
        ss << "pending";
    }
    ss << ")";
    return ss.str();
}

std::shared_ptr<void> TDIO_Input::getResultValue() {
    return std::make_shared<__u16>(this->params.value);
}

// --------------------------------------------------------------------------
// TDIO_Output Implementation
// --------------------------------------------------------------------------
TDIO_Output::TDIO_Output(DataItemIds id, const TBytes &data)
    : TDataItem<TDIO_OutputParams>(id, data)
{
    if(data.size() >= 2) {
        this->params.value = static_cast<__u16>(static_cast<__u16>(data[0]) | (static_cast<__u16>(data[1]) << 8));
    }
}

TDIO_Output::TDIO_Output(DataItemIds id, __u16 value)
    : TDataItem<TDIO_OutputParams>(id, {})
{
    this->params.value = value;
}

TDataItemBase &TDIO_Output::Go() {
    // Write the digital output state.
    out(ofsDioOutputs, static_cast<__u32>(this->params.value));
    return *this;
}

TBytes TDIO_Output::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(static_cast<__u8>(this->params.value & 0xFF));
    bytes.push_back(static_cast<__u8>((this->params.value >> 8) & 0xFF));
    return bytes;
}

std::string TDIO_Output::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_Output(value=0x" << std::hex << std::setw(4) << std::setfill('0')
       << this->params.value << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_ConfigureBit Implementation
// --------------------------------------------------------------------------
TDIO_ConfigureBit::TDIO_ConfigureBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_ConfigureBitParams>(id, data)
{
    if (data.size() >= 2) {
        this->params.bitNumber = data[0];
        this->params.direction = data[1];
    }
}
TDIO_ConfigureBit::TDIO_ConfigureBit(DataItemIds id, __u8 bitNumber, __u8 direction)
    : TDataItem<DIO_ConfigureBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
    this->params.direction = direction;
}
TDataItemBase &TDIO_ConfigureBit::Go() {
    __u32 regValue = in(ofsDioDirections);
    regValue &= ~(1 << this->params.bitNumber);
    if (this->params.direction) {
        regValue |= (1 << this->params.bitNumber);
    }
    out(ofsDioDirections, regValue);
    return *this;
}
TBytes TDIO_ConfigureBit::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    bytes.push_back(this->params.direction);
    return bytes;
}
std::string TDIO_ConfigureBit::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_ConfigureBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber)
       << ", direction=" << static_cast<int>(this->params.direction) << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_InputBit Implementation
// --------------------------------------------------------------------------
TDIO_InputBit::TDIO_InputBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_InputBitParams>(id, data)
{
    if (data.size() >= 1) {
        this->params.bitNumber = data[0];
    }
}
TDIO_InputBit::TDIO_InputBit(DataItemIds id, __u8 bitNumber)
    : TDataItem<DIO_InputBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
}
TDataItemBase &TDIO_InputBit::Go() {
    __u32 regValue = in(ofsDioInputs);
    this->params.bitValue = (__u8)((regValue >> this->params.bitNumber) & 1);
    return *this;
}
TBytes TDIO_InputBit::calcPayload(bool bAsReply) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    if (bAsReply) {
        bytes.push_back(this->params.bitValue);
    }
    return bytes;
}
std::string TDIO_InputBit::AsString(bool bAsReply) {
    std::stringstream ss;
    ss << "TDIO_InputBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber);
    if (bAsReply) {
        ss << ", value=" << static_cast<int>(this->params.bitValue);
    }
    ss << ")";
    return ss.str();
}
std::shared_ptr<void> TDIO_InputBit::getResultValue() {
    return std::make_shared<__u8>(this->params.bitValue);
}

// --------------------------------------------------------------------------
// TDIO_OutputBit Implementation
// --------------------------------------------------------------------------
TDIO_OutputBit::TDIO_OutputBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_OutputBitParams>(id, data)
{
    if (data.size() >= 2) {
        this->params.bitNumber = data[0];
        this->params.value = data[1];
    }
}
TDIO_OutputBit::TDIO_OutputBit(DataItemIds id, __u8 bitNumber, __u8 value)
    : TDataItem<DIO_OutputBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
    this->params.value = value;
}
TDataItemBase &TDIO_OutputBit::Go() {
    __u32 regValue = in(ofsDioOutputs);
    regValue &= ~(1 << this->params.bitNumber);
    if (this->params.value) {
        regValue |= (1 << this->params.bitNumber);
    }
    out(ofsDioOutputs, regValue);
    return *this;
}
TBytes TDIO_OutputBit::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    bytes.push_back(this->params.value);
    return bytes;
}
std::string TDIO_OutputBit::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "DIO_OutputBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber)
       << ", value=" << static_cast<int>(this->params.value) << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_ClearBit Implementation
// --------------------------------------------------------------------------
TDIO_ClearBit::TDIO_ClearBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_ClearBitParams>(id, data)
{
    if (data.size() >= 1) {
        this->params.bitNumber = data[0];
    }
}
TDIO_ClearBit::TDIO_ClearBit(DataItemIds id, __u8 bitNumber)
    : TDataItem<DIO_ClearBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
}
TDataItemBase &TDIO_ClearBit::Go() {
    __u32 regValue = in(ofsDioOutputs);
    regValue &= ~(1 << this->params.bitNumber);
    out(ofsDioOutputs, regValue);
    return *this;
}
TBytes TDIO_ClearBit::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    return bytes;
}
std::string TDIO_ClearBit::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_ClearBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber) << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_SetBit Implementation
// --------------------------------------------------------------------------
TDIO_SetBit::TDIO_SetBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_SetBitParams>(id, data)
{
    if (data.size() >= 1) {
        this->params.bitNumber = data[0];
    }
}
TDIO_SetBit::TDIO_SetBit(DataItemIds id, __u8 bitNumber)
    : TDataItem<DIO_SetBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
}
TDataItemBase &TDIO_SetBit::Go() {
    __u32 regValue = in(ofsDioOutputs);
    regValue |= (1 << this->params.bitNumber);
    out(ofsDioOutputs, regValue);
    return *this;
}
TBytes TDIO_SetBit::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    return bytes;
}
std::string TDIO_SetBit::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_SetBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber) << ")";
    return ss.str();
}

// --------------------------------------------------------------------------
// TDIO_ToggleBit Implementation
// --------------------------------------------------------------------------
TDIO_ToggleBit::TDIO_ToggleBit(DataItemIds id, const TBytes &data)
    : TDataItem<DIO_ToggleBitParams>(id, data)
{
    if (data.size() >= 1) {
        this->params.bitNumber = data[0];
    }
}
TDIO_ToggleBit::TDIO_ToggleBit(DataItemIds id, __u8 bitNumber)
    : TDataItem<DIO_ToggleBitParams>(id, {})
{
    this->params.bitNumber = bitNumber;
}
TDataItemBase &TDIO_ToggleBit::Go() {
    __u32 regValue = in(ofsDioOutputs);
    regValue ^= (1 << this->params.bitNumber);
    out(ofsDioOutputs, regValue);
    return *this;
}
TBytes TDIO_ToggleBit::calcPayload(bool /*bAsReply*/) {
    TBytes bytes;
    bytes.push_back(this->params.bitNumber);
    return bytes;
}
std::string TDIO_ToggleBit::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDIO_ToggleBit(bitNumber=" << std::dec << static_cast<int>(this->params.bitNumber) << ")";
    return ss.str();
}
