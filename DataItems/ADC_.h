#pragma once

#include "TDataItem.h"

// -------------------- Parameter Structs --------------------

// TADC_BaseClock: Just stores a single field "baseClock"
struct ADC_BaseClockParams {
    __u32 baseClock = 25000000; // default
};

// TADC_StreamStart: Just store the connection ID
struct ADC_StreamStartParams {
    int argConnectionID = -1;
};

// TADC_StreamStop: No fields needed
struct ADC_StreamStopParams {};

// -------------------- TADC_BaseClock --------------------
class TADC_BaseClock : public TDataItem<ADC_BaseClockParams>
{
public:
    TADC_BaseClock()
    : TDataItem<ADC_BaseClockParams>(DataItemIds::ADC_BaseClock, {})
    {
    // optional: parse an empty TBytes
    }
    // Constructor that checks incoming byte size
    // (If you want to pass 0 or 4 bytes.)
    TADC_BaseClock(DataItemIds dId, const TBytes &FromBytes);

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TADC_BaseClock &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// -------------------- TADC_StreamStart --------------------
class TADC_StreamStart : public TDataItem<ADC_StreamStartParams>
{
public:
    // You can define a constructor if you want partial parsing:
    TADC_StreamStart(DataItemIds dId, const TBytes &FromBytes);

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TADC_StreamStart &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// -------------------- TADC_StreamStop --------------------
class TADC_StreamStop : public TDataItem<ADC_StreamStopParams>
{
public:
    // No fields, so only the base constructor needed
    TADC_StreamStop(DataItemIds dId, const TBytes &FromBytes)
      : TDataItem<ADC_StreamStopParams>(dId, FromBytes) {}

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TADC_StreamStop &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};

