#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include "TDataItem.h"

// -------------------- TADC_BaseClock --------------------
struct ADC_BaseClockParams {
    __u32 baseClock = 25000000; // default
};
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
    TADC_BaseClock(DataItemIds id, const TBytes &FromBytes);

    // Overridden methods
    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TADC_BaseClock &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// -------------------- TADC_StreamStart --------------------
struct ADC_StreamStartParams {
    __u32 argConnectionID = (__u32)-1;
};

class TADC_StreamStart : public TDataItem<ADC_StreamStartParams>
{
public:
    TADC_StreamStart(DataItemIds id, TBytes FromBytes);

    virtual TADC_StreamStart &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// -------------------- TADC_StreamStop --------------------

class TADC_StreamStop : public TDataItemBase
{
public:
    TADC_StreamStop(DataItemIds id, TBytes FromBytes);

    virtual TBytes calcPayload(bool bAsReply=false) override;
    virtual TADC_StreamStop &Go() override;
    virtual std::string AsString(bool bAsReply = false) override;
};


// ADC_Differential1: Configure a single channel group as differential or single‐ended.
// In the ADC range register, bit 3 determines the input type: 0 = differential, 1 = single‐ended.
struct ADC_Differential1Params {
    __u8 channelGroup;     // Which ADC channel group (0–7)
    __u8 singleEnded;      // 1 for single-ended, 0 for differential.
};

class TADC_Differential1 : public TDataItem<ADC_Differential1Params> {
public:
    TADC_Differential1(DataItemIds id, const TBytes &data);
    TADC_Differential1(DataItemIds id, __u8 channelGroup, __u8 singleEnded);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

struct ADC_DifferentialAllParams {
    __u8 settings[8];  // One per channel group.
};

class TADC_DifferentialAll : public TDataItem<ADC_DifferentialAllParams> {
public:
    TADC_DifferentialAll(DataItemIds id, const TBytes &data);
    TADC_DifferentialAll(DataItemIds id, const __u8 settings[8]);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// ADC_Range1: Set the range for a single channel group.
// The value written (e.g. bmAdcRange_u10V, etc.) is written to (ofsAdcRange + channelGroup).
struct ADC_Range1Params {
    __u8 channelGroup;
    __u8 range;  // The bitmask value representing the range.
};

class TADC_Range1 : public TDataItem<ADC_Range1Params> {
public:
    TADC_Range1(DataItemIds id, const TBytes &data);
    TADC_Range1(DataItemIds id, __u8 channelGroup, __u8 range);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

struct ADC_RangeAllParams {
    __u8 ranges[8];  // One range value per channel group.
};

class TADC_RangeAll : public TDataItem<ADC_RangeAllParams> {
public:
    TADC_RangeAll(DataItemIds id, const TBytes &data);
    TADC_RangeAll(DataItemIds id, const __u8 ranges[8]);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// ADC Span (Scale) DataItems
// --------------------------------------------------------------------------

// ADC_Span1: Write a single floating-point scale calibration value to the scale register for one range.
// Hardware register address: ofsAdcCalScale + (rangeIndex * ofsAdcCalScaleStride)
struct ADC_Span1Params {
    __u8 rangeIndex; // 0..7
    float scale;
};

class TADC_Scale1 : public TDataItem<ADC_Span1Params> {
public:
    TADC_Scale1(DataItemIds id, const TBytes &data);
    TADC_Scale1(DataItemIds id, __u8 rangeIndex, float scale);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// ADC_SpanAll: Write scale calibration for all 8 ranges.
struct ADC_SpanAllParams {
    float scales[8];
};

class TADC_ScaleAll : public TDataItem<ADC_SpanAllParams> {
public:
    TADC_ScaleAll(DataItemIds id, const TBytes &data);
    TADC_ScaleAll(DataItemIds id, const float scales[8]);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// ADC Offset DataItems
// --------------------------------------------------------------------------

// ADC_Offset1: Write a single offset calibration value for one range.
struct ADC_Offset1Params {
    __u8 rangeIndex;
    float offset;
};

class TADC_Offset1 : public TDataItem<ADC_Offset1Params> {
public:
    TADC_Offset1(DataItemIds id, const TBytes &data);
    TADC_Offset1(DataItemIds id, __u8 rangeIndex, float offset);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// ADC_OffsetAll: Write offset calibration for all 8 ranges.
struct ADC_OffsetAllParams {
    float offsets[8];
};

class TADC_OffsetAll : public TDataItem<ADC_OffsetAllParams> {
public:
    TADC_OffsetAll(DataItemIds id, const TBytes &data);
    TADC_OffsetAll(DataItemIds id, const float offsets[8]);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// --------------------------------------------------------------------------
// ADC Calibration DataItems (both scale and offset together)
// --------------------------------------------------------------------------

// ADC_Calibration1: Write scale and offset for one range in one packet.
struct ADC_Calibration1Params {
    __u8 rangeIndex;
    float scale;
    float offset;
};

class TADC_Calibration1 : public TDataItem<ADC_Calibration1Params> {
public:
    TADC_Calibration1(DataItemIds id, const TBytes &data);
    TADC_Calibration1(DataItemIds id, __u8 rangeIndex, float scale, float offset);

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

// ADC_CalibrationAll: Write scale and offset for all 8 ranges in one packet,
// with values interleaved as: scale0, offset0, scale1, offset1, …, scale7, offset7.
struct ADC_CalibrationAllParams {
    float scales[8];
    float offsets[8];
};

class TADC_CalibrationAll : public TDataItem<ADC_CalibrationAllParams> {
public:
    TADC_CalibrationAll(DataItemIds id, const TBytes &data); // Constructor from interleaved byte payload
    TADC_CalibrationAll(DataItemIds id, const float scales[8], const float offsets[8]); // Explicit parameter constructor: scales and offsets provided separately.

    virtual TDataItemBase &Go() override;
    virtual TBytes calcPayload(bool bAsReply = false) override;
    virtual std::string AsString(bool bAsReply = false) override;
};

