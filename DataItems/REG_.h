#pragma once

#include "../utilities.h"
#include "TDataItem.h"

#pragma pack(push, 1)

// This is the struct for TREG_Write1 usage
typedef struct
{
    __u8 offset;
    __u8 width;
    __u32 value;
} REG_Write;

typedef std::vector<REG_Write> REG_WriteList;

#pragma pack(pop)

// -------------- Parameter structs --------------
struct REG_Read1Params {
    // offset + width + value
    int offset   = 0;
    int width    = 0;
    __u32 regVal = 0;  // “Value” from hardware read
};

struct REG_WritesParams {
    REG_WriteList Writes;
    // You can store other data if needed
};

// -------------- TREG_Read1 --------------
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

protected:
    // Now references `this->params.Writes` instead of a local `Writes`
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

