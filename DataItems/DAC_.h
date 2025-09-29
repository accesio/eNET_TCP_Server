#pragma once

#include "TDataItem.h"
#include "../utilities.h"

static constexpr __u32 RC_UNIPOLAR_5   = 0x35303055; // 'U005'
static constexpr __u32 RC_BIPOLAR_5    = 0x3530E142; // 'B±05'
static constexpr __u32 RC_UNIPOLAR_10  = 0x30313055; // 'U010'
static constexpr __u32 RC_BIPOLAR_10   = 0x3031E142; // 'B±10'

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
    TDAC_Output(DataItemIds id, const TBytes &FromBytes)
        : TDataItem(id, FromBytes)
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

// 2) TDAC_OutputV
// ==============
class TDAC_OutputV : public TDataItem<DAC_OutputParams>
{
public:
    // Constructor that does partial parse logic
    // (like your old code)
    explicit TDAC_OutputV(const TBytes &bytes);

    // Additional constructor for the dictionary/factory
    TDAC_OutputV(DataItemIds id, const TBytes &FromBytes)
        : TDataItem(id, FromBytes)
    {
        // We rely on the main constructor for partial parsing
        // So forward there:
        parseBytes(FromBytes);
    }

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual std::string AsString(bool bAsReply=false) override;
    virtual TDAC_OutputV &Go() override;

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
    TDAC_Range1(DataItemIds id, const TBytes &FromBytes)
        : TDataItem(id, FromBytes)
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
