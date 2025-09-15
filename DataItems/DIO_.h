#pragma once

#include "TDataItem.h"
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>

struct DIO_ConfigureBitParams {
    __u8 bitNumber;  // Which digital bit (0..15)
    __u8 direction;  // 1 = input, 0 = output
};

struct DIO_InputBitParams {
    __u8 bitNumber;
    __u8 bitValue;   // Result after reading (0 or 1)
};

struct DIO_OutputBitParams {
    __u8 bitNumber;
    __u8 value;      // Desired output value (0 or 1)
};

struct DIO_ClearBitParams {
    __u8 bitNumber;
};

struct DIO_SetBitParams {
    __u8 bitNumber;
};

struct DIO_ToggleBitParams {
    __u8 bitNumber;
};

// For configuring all 16 digital channels (direction only).
// Each bit in 'value' represents a channel: 1=input, 0=output.
struct TDIO_ConfigureParams {
    __u16 value;
};

// For reading digital input (16 bits).
struct TDIO_InputParams {
    __u16 value;  // This will be filled with the 16-bit input state.
};

// For writing digital output (16 bits).
struct TDIO_OutputParams {
    __u16 value;
};

// --------------------------------------------------------------------------
// TDIO_Configure: Configures directions for all 16 digital I/O bits.
// Writes the 16-bit word to the direction register (ofsDioDirections).
// --------------------------------------------------------------------------
class TDIO_Configure : public TDataItem<TDIO_ConfigureParams> {
public:
    TDIO_Configure(DataItemIds id, const TBytes &data);
    TDIO_Configure(DataItemIds id, __u16 value);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// TDIO_Input: Reads all 16 digital input bits.
// Reads from the input register (ofsDioInputs) and returns the 16-bit value.
// --------------------------------------------------------------------------
class TDIO_Input : public TDataItem<TDIO_InputParams> {
public:
    TDIO_Input(DataItemIds id, const TBytes &data);
    TDIO_Input(DataItemIds id);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
    virtual std::shared_ptr<void> getResultValue();
};

// --------------------------------------------------------------------------
// TDIO_Output: Writes a 16-bit value to the digital outputs.
// Writes to the output register (ofsDioOutputs).
// --------------------------------------------------------------------------
class TDIO_Output : public TDataItem<TDIO_OutputParams> {
public:
    TDIO_Output(DataItemIds id, const TBytes &data);
    TDIO_Output(DataItemIds id, __u16 value);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// DIO_ConfigureBit: Configures a single digital bit's direction.
// Uses the direction register (ofsDioDirections).
// --------------------------------------------------------------------------
class TDIO_ConfigureBit : public TDataItem<DIO_ConfigureBitParams> {
public:
    TDIO_ConfigureBit(DataItemIds id, const TBytes &data);
    TDIO_ConfigureBit(DataItemIds id, __u8 bitNumber, __u8 direction);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// DIO_InputBit: Reads a single bit from the digital input register (ofsDioInputs).
// --------------------------------------------------------------------------
class TDIO_InputBit : public TDataItem<DIO_InputBitParams> {
public:
    TDIO_InputBit(DataItemIds id, const TBytes &data);
    TDIO_InputBit(DataItemIds id, __u8 bitNumber);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
    virtual std::shared_ptr<void> getResultValue();
};

// --------------------------------------------------------------------------
// DIO_OutputBit: Writes a single bit value to the digital output register (ofsDioOutputs).
// --------------------------------------------------------------------------
class TDIO_OutputBit : public TDataItem<DIO_OutputBitParams> {
public:
    TDIO_OutputBit(DataItemIds id, const TBytes &data);
    TDIO_OutputBit(DataItemIds id, __u8 bitNumber, __u8 value);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// DIO_ClearBit: Clears (sets to 0) a single digital output bit in ofsDioOutputs.
// --------------------------------------------------------------------------
class TDIO_ClearBit : public TDataItem<DIO_ClearBitParams> {
public:
    TDIO_ClearBit(DataItemIds id, const TBytes &data);
    TDIO_ClearBit(DataItemIds id, __u8 bitNumber);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// DIO_SetBit: Sets (to 1) a single digital output bit in ofsDioOutputs.
// --------------------------------------------------------------------------
class TDIO_SetBit : public TDataItem<DIO_SetBitParams> {
public:
    TDIO_SetBit(DataItemIds id, const TBytes &data);
    TDIO_SetBit(DataItemIds id, __u8 bitNumber);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// DIO_ToggleBit: Toggles a single digital output bit in ofsDioOutputs.
// --------------------------------------------------------------------------
class TDIO_ToggleBit : public TDataItem<DIO_ToggleBitParams> {
public:
    TDIO_ToggleBit(DataItemIds id, const TBytes &data);
    TDIO_ToggleBit(DataItemIds id, __u8 bitNumber);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};
