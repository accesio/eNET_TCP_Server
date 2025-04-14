#pragma once

#include "../utilities.h"
#include "TDataItem.h"

#pragma pack(push, 1)



// -------------- TREG_Read1 --------------
struct REG_Read1Params {
    // offset + width + value
    int offset   = 0;
    int width    = 0;
    __u32 regVal = 0;  // “Value” from hardware read
};
class TREG_Read1 : public TDataItem<REG_Read1Params>
{
public:
    static TError validateDataItemPayload(DataItemIds DataItemID, TBytes Data);

    // Constructors
    explicit TREG_Read1(const TBytes &data);
    TREG_Read1();
    TREG_Read1(DataItemIds DId, int ofs);
    TREG_Read1(DataItemIds DId, const TBytes &FromBytes);

    // Core methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TREG_Read1 &Go() override;
    virtual std::shared_ptr<void> getResultValue();
    virtual std::string AsString(bool bAsReply = false) override;

    // Helper
    TREG_Read1 &setOffset(int ofs);

private:
    // No separate Value field; it is in `this->params.regVal`
};

// -------------- TREG_Writes --------------
typedef struct
{
    __u8 offset;
    __u8 width;
    __u32 value;
} REG_Write;

typedef std::vector<REG_Write> REG_WriteList;
struct REG_WritesParams {
    REG_WriteList Writes;
    // You can store other data if needed
};

// Abstract base class for TREG_WriteAll-type items
class TREG_Writes : public TDataItem<REG_WritesParams>
{
public:
    TREG_Writes(DataItemIds DId);
    TREG_Writes();
    virtual ~TREG_Writes();

    explicit TREG_Writes(const TBytes &buf);
    TREG_Writes(DataItemIds DId, const TBytes &FromBytes);

    virtual TREG_Writes &Go() override;
    TREG_Writes &addWrite(__u8 w, int ofs, __u32 value);
    virtual std::string AsString(bool bAsReply=false) override;
};



// -------------- TREG_Write1 --------------

class TREG_Write1 : public TREG_Writes
{
public:
    static TError validateDataItemPayload(DataItemIds DataItemID, TBytes Data);

    TREG_Write1(DataItemIds ID, const TBytes &buf);
    virtual ~TREG_Write1();

    virtual TBytes calcPayload(bool bAsReply=false) override;
};



// --------------------------------------------------------------------------
// Parameter structures for individual-bit operations
// --------------------------------------------------------------------------
struct REG_ReadBitParams {
    __u8 offset;    // Register offset to read from
    __u8 bitIndex;  // Bit index within the register
    __u8 bitValue;  // Result (0 or 1) after reading
};

struct REG_WriteBitParams {
    __u8 offset;    // Register offset to write to
    __u8 bitIndex;  // Bit index within the register
    __u8 value;     // Bit value to write (0 or 1)
};

struct REG_ClearBitParams {
    __u8 offset;    // Register offset
    __u8 bitIndex;  // Bit index to clear
};

struct REG_SetBitParams {
    __u8 offset;    // Register offset
    __u8 bitIndex;  // Bit index to set
};

struct REG_ToggleBitParams {
    __u8 offset;    // Register offset to modify
    __u8 bitIndex;  // Bit index within the register to toggle
};

// --------------------------------------------------------------------------
// Class declarations for individual-bit TREG_ data items
// --------------------------------------------------------------------------

// TREG_ReadBit: Reads the full register and extracts the specified bit.
class TREG_ReadBit : public TDataItem<REG_ReadBitParams> {
public:
    // Constructors: one from raw bytes and one from explicit parameters.
    TREG_ReadBit(DataItemIds DId, const TBytes &data);
    TREG_ReadBit(DataItemIds DId, __u8 offset, __u8 bitIndex);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
    virtual std::shared_ptr<void> getResultValue();
};

// TREG_WriteBit: Reads the register, sets or clears the specified bit, and writes back.
class TREG_WriteBit : public TDataItem<REG_WriteBitParams> {
public:
    TREG_WriteBit(DataItemIds DId, const TBytes &data);
    TREG_WriteBit(DataItemIds DId, __u8 offset, __u8 bitIndex, __u8 value);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// TREG_ClearBit: Clears a single bit (sets it to 0) in a register.
class TREG_ClearBit : public TDataItem<REG_ClearBitParams> {
    public:
        // Constructors: one from raw bytes and one from explicit parameters.
        TREG_ClearBit(DataItemIds DId, const TBytes &data);
        TREG_ClearBit(DataItemIds DId, __u8 offset, __u8 bitIndex);

        virtual TDataItemBase &Go() override;
        virtual TBytes calcPayload(bool bAsReply = false) override;
        virtual std::string AsString(bool bAsReply = false) override;
    };

    // TREG_SetBit: Sets a single bit (to 1) in a register.
    class TREG_SetBit : public TDataItem<REG_SetBitParams> {
    public:
        TREG_SetBit(DataItemIds DId, const TBytes &data);
        TREG_SetBit(DataItemIds DId, __u8 offset, __u8 bitIndex);

        virtual TDataItemBase &Go() override;
        virtual TBytes calcPayload(bool bAsReply = false) override;
        virtual std::string AsString(bool bAsReply = false) override;
    };

// TREG_ToggleBit: Reads the register, toggles the specified bit, and writes back.
class TREG_ToggleBit : public TDataItem<REG_ToggleBitParams> {
public:
    TREG_ToggleBit(DataItemIds DId, const TBytes &data);
    TREG_ToggleBit(DataItemIds DId, __u8 offset, __u8 bitIndex);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};