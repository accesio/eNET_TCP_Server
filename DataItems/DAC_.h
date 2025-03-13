#pragma once

#include "TDataItem.h"
#include "../utilities.h"

// 1) Parameter structs
// ====================

// For TDAC_Output
struct DAC_OutputParams {
    __u8  dacChannel = 0;
    __u16 dacCounts  = 0;
};

// For TDAC_Range1
struct DAC_RangeParams {
    __u8  dacChannel = 0;
    __u32 dacRange   = 0;
};

// 2) TDAC_Output
// ==============
class TDAC_Output : public TDataItem<DAC_OutputParams>
{
public:
    // Constructor that does partial parse logic
    // (like your old code)
    explicit TDAC_Output(const TBytes &bytes);

    // Additional constructor for the dictionary/factory
    TDAC_Output(DataItemIds DId, const TBytes &FromBytes)
        : TDataItem(DId, FromBytes)
    {
        // We rely on the main constructor for partial parsing
        // So forward there:
        parseBytes(FromBytes);
    }

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual std::string AsString(bool bAsReply=false) override;
    virtual TDAC_Output &Go() override;

private:
    // Helper to unify the partial parsing logic
    void parseBytes(const TBytes &bytes);
};

// 3) TDAC_Range1
// =============
class TDAC_Range1 : public TDataItem<DAC_RangeParams>
{
public:
    // Constructors
    explicit TDAC_Range1(const TBytes &bytes);

    // For dictionary/factory usage
    TDAC_Range1(DataItemIds DId, const TBytes &FromBytes)
        : TDataItem(DId, FromBytes)
    {
        parseBytes(FromBytes);
    }

    // Overridden
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual std::string AsString(bool bAsReply = false) override;
    virtual TDAC_Range1 &Go() override;

private:
    // partial parse logic
    void parseBytes(const TBytes &bytes);
};
