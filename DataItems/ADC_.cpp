#include <cmath>

#include "ADC_.h"
#include "../apci.h"
#include "../logging.h"
#include "../eNET-AIO16-16F.h"
#include "../adc.h"
#include "../config.h"

extern int apci;

TADC_BaseClock::TADC_BaseClock(DataItemIds id, const TBytes &buf)
    : TDataItem<ADC_BaseClockParams>(id, buf)
{
    // You previously had a constructor that validated 0 or 4 bytes
    GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);

    // Optionally parse the 4 bytes into this->params.baseClock if you want
    // If you do:
    if (buf.size() == 4)
    {
        // e.g.:
        this->params.baseClock = *reinterpret_cast<const __u32 *>(buf.data());
    }
}

TBytes TADC_BaseClock::calcPayload(bool bAsReply)
{
    TBytes bytes;
    stuff(bytes, this->params.baseClock);
    Trace("TADC_BaseClock::calcPayload built: ", bytes);
    return bytes;
}

TADC_BaseClock &TADC_BaseClock::Go()
{
    Trace("ADC_BaseClock Go() reading hardware");
    // Instead of a separate field, we do:
    this->params.baseClock = in(ofsAdcBaseClock);
    return *this;
}

std::string TADC_BaseClock::AsString(bool bAsReply)
{
    std::stringstream dest;
    dest << "ADC_BaseClock()" << " ? " << this->params.baseClock;
    return dest.str();
}

//  TADC_StreamStart

TADC_StreamStart::TADC_StreamStart(DataItemIds id, TBytes FromBytes)
    : TDataItem<ADC_StreamStartParams>(id, FromBytes)
{
    if (FromBytes.size() == 4)
    {
        int cid = *reinterpret_cast<const int *>(FromBytes.data());
        if (AdcStreamingConnection == -1)
        {
            AdcStreamingConnection = cid;
            this->params.argConnectionID = cid;
        }
        else
        {
            Error("ADC Busy, already streaming on Connection: " + std::to_string(AdcStreamingConnection));
            throw std::logic_error("ADC Busy already on Connection: " + std::to_string(AdcStreamingConnection));
        }
        Trace("AdcStreamingConnection: " + std::to_string(AdcStreamingConnection));
    }
}

TADC_StreamStart &TADC_StreamStart::Go()
{
    Debug("ADC_StreamStart::Go(), ADC Streaming Data will be sent on ConnectionID: " + std::to_string(AdcStreamingConnection));

    // Example code from your snippet
    auto status = apciDmaTransferSize(RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
    if (status)
    {
        Error("Error setting apciDmaTransferSize: " + std::to_string(status));
        throw std::logic_error(err_msg[-status]);
    }

    AdcStreamTerminate = 0;
    if (AdcWorkerThreadID == -1)
    {
        int rc = pthread_create(&worker_thread, NULL, &worker_main, &AdcStreamingConnection);
        if (rc != 0) {
            Error("ADC_StreamStart::Go(): pthread_create(worker) failed: " +
                std::to_string(rc) + ", " + strerror(rc));
            throw std::logic_error("failed to start ADC worker thread");
        }
        AdcWorkerThreadID = 0; // “running”
    }

    apciDmaStart();
    Debug("ADC_StreamStart::Go(): apciDmaStart() called");
    return *this;
}

std::string TADC_StreamStart::AsString(bool bAsReply)
{
    std::string msg = this->getDIdDesc(this->DId);
    if (bAsReply)
    {
        msg += ", ConnectionID = " + to_hex<int>(this->params.argConnectionID);
    }
    return msg;
}

//  TADC_StreamStop
TADC_StreamStop::TADC_StreamStop(DataItemIds id, TBytes bytes)
    : TDataItemBase(id)
{
}

TADC_StreamStop &TADC_StreamStop::Go()
{
    Debug("ADC_StreamStop::Go(): requesting stop; conn=" + std::to_string(AdcStreamingConnection));
    AdcStreamTerminate = 1;
    apciCancelWaitForIRQ();
    Debug("ADC_StreamStop::Go() exiting");
    return *this;
}
TBytes TADC_StreamStop::calcPayload(bool bAsReply)
{
    (void)bAsReply;
    return TBytes{};   // no payload on replies for Stop
}

std::string TADC_StreamStop::AsString(bool bAsReply)
{
    (void)bAsReply;
    return getDIdDesc(this->DId);
}
//
// TADC_Differential1 Implementation
//
TADC_Differential1::TADC_Differential1(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_Differential1Params>(id, data)
{
    if (data.size() >= 2)
    {
        this->params.channelGroup = data[0];
        this->params.singleEnded = data[1];
    }
}

TADC_Differential1::TADC_Differential1(DataItemIds id, __u8 channelGroup, __u8 singleEnded)
    : TDataItem<ADC_Differential1Params>(id, {})
{
    this->params.channelGroup = channelGroup;
    this->params.singleEnded = singleEnded;
}

TDataItemBase &TADC_Differential1::Go()
{
    // Read current ADC range register for the channel group.
    __u8 reg = static_cast<__u8>(in(ofsAdcRange + this->params.channelGroup));
    // Bit 3 determines single-ended (1) vs differential (0)
    if (this->params.singleEnded)
        reg |= (1 << 3);
    else
        reg &= (__u8)(~(1 << 3));
    out(ofsAdcRange + this->params.channelGroup, reg);
    return *this;
}
TBytes TADC_Differential1::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    bytes.push_back(this->params.channelGroup);
    bytes.push_back(this->params.singleEnded);
    return bytes;
}
std::string TADC_Differential1::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_Differential1(channelGroup=" << std::dec << static_cast<int>(this->params.channelGroup)
       << ", " << (this->params.singleEnded ? "single-ended" : "differential") << ")";
    return ss.str();
}

//
// TADC_DifferentialAll Implementation
//
TADC_DifferentialAll::TADC_DifferentialAll(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_DifferentialAllParams>(id, data)
{
    if (data.size() >= 8)
    {
        for (int i = 0; i < 8; i++)
            this->params.settings[i] = data[i];
    }
}
TADC_DifferentialAll::TADC_DifferentialAll(DataItemIds id, const __u8 settings[8])
    : TDataItem<ADC_DifferentialAllParams>(id, {})
{
    for (int i = 0; i < 8; i++)
        this->params.settings[i] = settings[i];
}
TDataItemBase &TADC_DifferentialAll::Go()
{
    for (__u8 grp = 0; grp < 8; grp++)
    {
        __u8 reg = static_cast<__u8>(in(ofsAdcRange + grp));
        if (this->params.settings[grp])
            reg |= (1 << 3);
        else
            reg &= (__u8)(~(1 << 3));
        out(ofsAdcRange + grp, reg);
    }
    return *this;
}
TBytes TADC_DifferentialAll::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    for (int i = 0; i < 8; i++)
        bytes.push_back(this->params.settings[i]);
    return bytes;
}
std::string TADC_DifferentialAll::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_DifferentialAll(settings=[";
    for (int i = 0; i < 8; i++)
    {
        ss << static_cast<int>(this->params.settings[i]);
        if (i < 7)
            ss << ", ";
    }
    ss << "])";
    return ss.str();
}

//
// TADC_Range1 Implementation
//
TADC_Range1::TADC_Range1(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_Range1Params>(id, data)
{
    if (data.size() >= 2)
    {
        this->params.channelGroup = data[0];
        this->params.range = data[1];
    }
}
TADC_Range1::TADC_Range1(DataItemIds id, __u8 channelGroup, __u8 range)
    : TDataItem<ADC_Range1Params>(id, {})
{
    this->params.channelGroup = channelGroup;
    this->params.range = range;
}
TDataItemBase &TADC_Range1::Go()
{
    out(ofsAdcRange + this->params.channelGroup, this->params.range);
    return *this;
}
TBytes TADC_Range1::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    bytes.push_back(this->params.channelGroup);
    bytes.push_back(this->params.range);
    return bytes;
}
std::string TADC_Range1::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_Range1(channelGroup=" << std::dec << static_cast<int>(this->params.channelGroup)
       << ", range=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(this->params.range) << ")";
    return ss.str();
}

//
// TADC_RangeAll Implementation
//
TADC_RangeAll::TADC_RangeAll(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_RangeAllParams>(id, data)
{
    if (data.size() >= 8)
    {
        for (int i = 0; i < 8; i++)
            this->params.ranges[i] = data[i];
    }
}
TADC_RangeAll::TADC_RangeAll(DataItemIds id, const __u8 ranges[8])
    : TDataItem<ADC_RangeAllParams>(id, {})
{
    for (int i = 0; i < 8; i++)
        this->params.ranges[i] = ranges[i];
}
TDataItemBase &TADC_RangeAll::Go()
{
    for (int grp = 0; grp < 8; grp++)
    {
        out(ofsAdcRange + grp, this->params.ranges[grp]);
    }
    return *this;
}
TBytes TADC_RangeAll::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    for (int i = 0; i < 8; i++)
        bytes.push_back(this->params.ranges[i]);
    return bytes;
}
std::string TADC_RangeAll::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_RangeAll(ranges=[";
    for (int i = 0; i < 8; i++)
    {
        ss << "0x" << std::hex << static_cast<int>(this->params.ranges[i]);
        if (i < 7)
            ss << ", ";
    }
    ss << "])";
    return ss.str();
}

//
// TADC_Span1 Implementation (Scale calibration for one range)
//
TADC_Scale1::TADC_Scale1(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_Span1Params>(id, data)
{
    if (data.size() >= 5)
    {
        this->params.rangeIndex = data[0];
        // Next 4 bytes: float scale
        this->params.scale = *(reinterpret_cast<const float *>(&data[1]));
    }
}
TADC_Scale1::TADC_Scale1(DataItemIds id, __u8 rangeIndex, float scale)
    : TDataItem<ADC_Span1Params>(id, {})
{
    this->params.rangeIndex = rangeIndex;
    this->params.scale = scale;
}
TDataItemBase &TADC_Scale1::Go()
{
    // Write the scale to the appropriate calibration register.
    int addr = ofsAdcCalScale + (this->params.rangeIndex * ofsAdcCalScaleStride);
    out(addr, *reinterpret_cast<__u32 *>(&this->params.scale));
    // Update global configuration.
    Config.adcScaleCoefficients[this->params.rangeIndex] = this->params.scale;
    return *this;
}
TBytes TADC_Scale1::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    bytes.push_back(this->params.rangeIndex);
    const __u8 *p = reinterpret_cast<const __u8 *>(&this->params.scale);
    for (size_t i = 0; i < sizeof(float); i++)
        bytes.push_back(p[i]);
    return bytes;
}
std::string TADC_Scale1::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_Scale1(rangeIndex=" << std::dec << static_cast<int>(this->params.rangeIndex)
       << ", scale=" << this->params.scale << ")";
    return ss.str();
}

//
// TADC_ScaleAll Implementation
//
TADC_ScaleAll::TADC_ScaleAll(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_SpanAllParams>(id, data)
{
    if (data.size() >= 8 * sizeof(float))
    {
        for (int i = 0; i < 8; i++)
        {
            this->params.scales[i] = *(reinterpret_cast<const float *>(&data[i * sizeof(float)]));
        }
    }
}
TADC_ScaleAll::TADC_ScaleAll(DataItemIds id, const float scales[8])
    : TDataItem<ADC_SpanAllParams>(id, {})
{
    for (int i = 0; i < 8; i++)
        this->params.scales[i] = scales[i];
}
TDataItemBase &TADC_ScaleAll::Go()
{
    for (int i = 0; i < 8; i++)
    {
        int addr = ofsAdcCalScale + (i * ofsAdcCalScaleStride);
        out(addr, *reinterpret_cast<__u32 *>(&this->params.scales[i]));
        Config.adcScaleCoefficients[i] = this->params.scales[i];
    }
    return *this;
}
TBytes TADC_ScaleAll::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    for (int i = 0; i < 8; i++)
    {
        const __u8 *p = reinterpret_cast<const __u8 *>(&this->params.scales[i]);
        for (size_t j = 0; j < sizeof(float); j++)
            bytes.push_back(p[j]);
    }
    return bytes;
}
std::string TADC_ScaleAll::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_ScaleAll(scales=[";
    for (int i = 0; i < 8; i++)
    {
        ss << this->params.scales[i];
        if (i < 7)
            ss << ", ";
    }
    ss << "])";
    return ss.str();
}

//
// TADC_Offset1 Implementation (Offset calibration for one range)
//
TADC_Offset1::TADC_Offset1(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_Offset1Params>(id, data)
{
    if (data.size() >= 5)
    {
        this->params.rangeIndex = data[0];
        this->params.offset = *(reinterpret_cast<const float *>(&data[1]));
    }
}
TADC_Offset1::TADC_Offset1(DataItemIds id, __u8 rangeIndex, float offset)
    : TDataItem<ADC_Offset1Params>(id, {})
{
    this->params.rangeIndex = rangeIndex;
    this->params.offset = offset;
}
TDataItemBase &TADC_Offset1::Go()
{
    int addr = ofsAdcCalOffset + (this->params.rangeIndex * ofsAdcCalOffsetStride);
    out(addr, *reinterpret_cast<__u32 *>(&this->params.offset));
    Config.adcOffsetCoefficients[this->params.rangeIndex] = this->params.offset;
    return *this;
}
TBytes TADC_Offset1::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    bytes.push_back(this->params.rangeIndex);
    const __u8 *p = reinterpret_cast<const __u8 *>(&this->params.offset);
    for (size_t i = 0; i < sizeof(float); i++)
        bytes.push_back(p[i]);
    return bytes;
}
std::string TADC_Offset1::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_Offset1(rangeIndex=" << std::dec << static_cast<int>(this->params.rangeIndex)
       << ", offset=" << this->params.offset << ")";
    return ss.str();
}

//
// TADC_OffsetAll Implementation
//
TADC_OffsetAll::TADC_OffsetAll(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_OffsetAllParams>(id, data)
{
    if (data.size() >= 8 * sizeof(float))
    {
        for (int i = 0; i < 8; i++)
        {
            this->params.offsets[i] = *(reinterpret_cast<const float *>(&data[i * sizeof(float)]));
        }
    }
}
TADC_OffsetAll::TADC_OffsetAll(DataItemIds id, const float offsets[8])
    : TDataItem<ADC_OffsetAllParams>(id, {})
{
    for (int i = 0; i < 8; i++)
        this->params.offsets[i] = offsets[i];
}
TDataItemBase &TADC_OffsetAll::Go()
{
    for (int i = 0; i < 8; i++)
    {
        int addr = ofsAdcCalOffset + (i * ofsAdcCalOffsetStride);
        out(addr, *reinterpret_cast<__u32 *>(&this->params.offsets[i]));
        Config.adcOffsetCoefficients[i] = this->params.offsets[i];
    }
    return *this;
}
TBytes TADC_OffsetAll::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    for (int i = 0; i < 8; i++)
    {
        const __u8 *p = reinterpret_cast<const __u8 *>(&this->params.offsets[i]);
        for (size_t j = 0; j < sizeof(float); j++)
            bytes.push_back(p[j]);
    }
    return bytes;
}
std::string TADC_OffsetAll::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_OffsetAll(offsets=[";
    for (int i = 0; i < 8; i++)
    {
        ss << this->params.offsets[i];
        if (i < 7)
            ss << ", ";
    }
    ss << "])";
    return ss.str();
}

//
// TADC_Calibration1 Implementation (both scale and offset for one range)
//
TADC_Calibration1::TADC_Calibration1(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_Calibration1Params>(id, data)
{
    if (data.size() >= 9)
    {
        this->params.rangeIndex = data[0];
        this->params.scale = *(reinterpret_cast<const float *>(&data[1]));
        this->params.offset = *(reinterpret_cast<const float *>(&data[5]));
    }
}
TADC_Calibration1::TADC_Calibration1(DataItemIds id, __u8 rangeIndex, float scale, float offset)
    : TDataItem<ADC_Calibration1Params>(id, {})
{
    this->params.rangeIndex = rangeIndex;
    this->params.scale = scale;
    this->params.offset = offset;
}
TDataItemBase &TADC_Calibration1::Go()
{
    int addrScale = ofsAdcCalScale + (this->params.rangeIndex * ofsAdcCalScaleStride);
    int addrOffset = ofsAdcCalOffset + (this->params.rangeIndex * ofsAdcCalOffsetStride);
    out(addrScale, *reinterpret_cast<__u32 *>(&this->params.scale));
    out(addrOffset, *reinterpret_cast<__u32 *>(&this->params.offset));
    Config.adcScaleCoefficients[this->params.rangeIndex] = this->params.scale;
    Config.adcOffsetCoefficients[this->params.rangeIndex] = this->params.offset;
    return *this;
}
TBytes TADC_Calibration1::calcPayload(bool /*bAsReply*/)
{
    TBytes bytes;
    bytes.push_back(this->params.rangeIndex);
    const __u8 *pScale = reinterpret_cast<const __u8 *>(&this->params.scale);
    for (size_t i = 0; i < sizeof(float); i++)
        bytes.push_back(pScale[i]);
    const __u8 *pOffset = reinterpret_cast<const __u8 *>(&this->params.offset);
    for (size_t i = 0; i < sizeof(float); i++)
        bytes.push_back(pOffset[i]);
    return bytes;
}
std::string TADC_Calibration1::AsString(bool /*bAsReply*/)
{
    std::stringstream ss;
    ss << "TADC_Calibration1(rangeIndex=" << std::dec << static_cast<int>(this->params.rangeIndex)
       << ", scale=" << this->params.scale
       << ", offset=" << this->params.offset << ")";
    return ss.str();
}

//
// TADC_CalibrationAll Implementation

// TADC_CalibrationAll Constructor (from interleaved TBytes)
//
TADC_CalibrationAll::TADC_CalibrationAll(DataItemIds id, const TBytes &data)
    : TDataItem<ADC_CalibrationAllParams>(id, data)
{
    // Expect interleaved scale/offset pairs: 16 floats = 16 * sizeof(float) bytes.
    if (data.size() >= 16 * sizeof(float))
    {
        const __u8 *base = data.data();
        for (int i = 0; i < 8; i++)
        {
            // Scale is at even index.
            this->params.scales[i] = *(reinterpret_cast<const float *>(base + (2 * i) * sizeof(float)));
            // Offset is at odd index.
            this->params.offsets[i] = *(reinterpret_cast<const float *>(base + (2 * i + 1) * sizeof(float)));
        }
    }
}

TADC_CalibrationAll::TADC_CalibrationAll(DataItemIds id, const float scales[8], const float offsets[8])
    : TDataItem<ADC_CalibrationAllParams>(id, {})
{
    for (int i = 0; i < 8; i++)
    {
        this->params.scales[i] = scales[i];
        this->params.offsets[i] = offsets[i];
    }
}

//
// Go(): Write calibration values for all 8 ranges
//
TDataItemBase &TADC_CalibrationAll::Go()
{
    for (int i = 0; i < 8; i++)
    {
        int addrScale = ofsAdcCalScale + (i * ofsAdcCalScaleStride);
        int addrOffset = ofsAdcCalOffset + (i * ofsAdcCalOffsetStride);
        out(addrScale, *reinterpret_cast<__u32 *>(&this->params.scales[i]));
        out(addrOffset, *reinterpret_cast<__u32 *>(&this->params.offsets[i]));
        Config.adcScaleCoefficients[i] = this->params.scales[i];
        Config.adcOffsetCoefficients[i] = this->params.offsets[i];
    }
    return *this;
}

//
// calcPayload(): Return the interleaved scale/offset pairs as bytes
//
TBytes TADC_CalibrationAll::calcPayload(bool bAsReply)
{
    TBytes bytes;
    for (int i = 0; i < 8; i++)
    {
        const __u8 *pScale = reinterpret_cast<const __u8 *>(&this->params.scales[i]);
        for (size_t j = 0; j < sizeof(float); j++)
        {
            bytes.push_back(pScale[j]);
        }
        const __u8 *pOffset = reinterpret_cast<const __u8 *>(&this->params.offsets[i]);
        for (size_t j = 0; j < sizeof(float); j++)
        {
            bytes.push_back(pOffset[j]);
        }
    }
    return bytes;
}

//
// AsString(): Provide an explicit string representation per range
//
std::string TADC_CalibrationAll::AsString(bool bAsReply)
{
    std::stringstream ss;
    ss << "TADC_CalibrationAll(";
    for (int i = 0; i < 8; i++)
    {
        ss << "range " << i << ": scale=" << this->params.scales[i]
           << ", offset=" << this->params.offsets[i];
        if (i < 7)
            ss << "; ";
    }
    ss << ")";
    return ss.str();
}

// ADC_ImmediateGetScanxxx) ------------------------------------

struct TAdcScanCfg
{
    __u8  startCh     = 0;
    __u8  endCh       = 0;
    __u8  oversamples = 0;

    __u32 nChannels   = 0;
    __u32 perChannel  = 0;   // = oversamples + 1
    __u32 expected    = 0;   // = nChannels * perChannel
};

static inline __u16 AdcRawCounts(__u32 raw)
{
    return static_cast<__u16>(raw & 0xFFFFu);
}

static inline __u32 AdcRawMeta(__u32 raw)
{
    // Upper 16 bits contain INV/SE/Gain/BIP/Channel/unused; keep them.
    return (raw & 0xFFFF0000u);
}

static inline __u8 AdcRawChannel(__u32 raw)
{
    return static_cast<__u8>((raw >> 20) & 0x7Fu);
}

struct TPerChAccum
{
    __u32 meta = 0x80000000u; // sentinel: INV set means "no data captured for this channel"
    __u64 sum = 0;
    __u32 seen = 0;           // how many samples seen for this channel this scan
    __u32 included = 0;       // how many samples contributed to sum (after drop-first policy)
    __u16 first = 0;
    bool firstSet = false;
};

static inline __u16 FinalizeAvgCounts(const TPerChAccum& a, const TAdcScanCfg& cfg)
{
    if (cfg.oversamples == 0)
        return a.firstSet ? a.first : static_cast<__u16>(0);

    if (a.included == 0)
        return static_cast<__u16>(0);

    const __u64 avg = (a.sum + (static_cast<__u64>(a.included) / 2u)) / static_cast<__u64>(a.included);
    return static_cast<__u16>(avg & 0xFFFFu);
}

static void AccumulateByChannel(const std::vector<__u32>& raw, const TAdcScanCfg& cfg, std::vector<TPerChAccum>& acc)
{
    acc.assign(cfg.nChannels, {});

    for (const __u32 v : raw)
    {
        if (v & 0x80000000u)
            continue; // INV: empty read; ignore

        const __u8 ch = AdcRawChannel(v);
        if (ch < cfg.startCh || ch > cfg.endCh)
            continue;

        const size_t idx = static_cast<size_t>(ch - cfg.startCh);
        if (idx >= acc.size())
            continue;

        TPerChAccum& a = acc[idx];
        if (a.seen >= cfg.perChannel)
            continue; // ignore extras

        if (a.meta == 0x80000000u)
            a.meta = AdcRawMeta(v);

        const __u16 c = AdcRawCounts(v);

        if (cfg.oversamples == 0)
        {
            a.first = c;
            a.firstSet = true;
            a.seen = cfg.perChannel; // mark done
            continue;
        }

        if (cfg.oversamples == 1)
        {
            a.sum += c;
            ++a.included;
            ++a.seen;
            continue;
        }

        // oversamples > 1: drop first, average the rest
        if (a.seen != 0)
        {
            a.sum += c;
            ++a.included;
        }
        ++a.seen;
    }
}

static inline TAdcScanCfg ADC_ReadScanCfg()
{
    TAdcScanCfg cfg;
    cfg.startCh     = static_cast<__u8>(in(ofsAdcStartChannel) & 0xFFu);   // +13
    cfg.endCh       = static_cast<__u8>(in(ofsAdcStopChannel)  & 0xFFu);   // +14
    cfg.oversamples = static_cast<__u8>(in(ofsAdcOversamples)  & 0xFFu);   // +15

    if (cfg.endCh >= cfg.startCh)
    {
        cfg.nChannels  = static_cast<__u32>(cfg.endCh - cfg.startCh + 1u);
        cfg.perChannel = static_cast<__u32>(cfg.oversamples) + 1u;
        cfg.expected   = cfg.nChannels * cfg.perChannel;
    }
    else
    {
        // Invalid configuration; expected stays 0.
        // (Return an error from the caller if you prefer.)
    }
    return cfg;
}



static inline float AdcRawToVolts(__u32 raw)
{
    // FIFO format summary (per register reference):
    // bit31 INV, bit30 SE, bits29:28 G1:G0, bit27 BIP, bits26:20 Channel, bits15:0 Counts.
    const bool inv = (raw & 0x80000000u) != 0;
    if (inv)
        return std::numeric_limits<float>::quiet_NaN();

    const __u8 gainCode = static_cast<__u8>((raw >> 28) & 0x03u);
    const bool bipolar  = (raw & (1u << 27)) != 0;
    const __u16 counts  = static_cast<__u16>(raw & 0xFFFFu);

    // Gain mapping per register reference wording: 00=>10V, 01=>5V, 10=>2.5V, 11=>1V (max input).
    double fsVolts = 10.0;
    switch (gainCode)
    {
        case 0: fsVolts = 10.0; break;
        case 1: fsVolts =  5.0; break;
        case 2: fsVolts =  2.5; break;
        case 3: fsVolts =  1.0; break;
    }

    // Counts are offset-binary. In bipolar ranges, 0x8000 is 0V.
    double v = 0.0;
    if (!bipolar)
    {
        v = (static_cast<double>(counts) * fsVolts) / 65535.0;
    }
    else
    {
        // offset-binary bipolar: 0x8000 is ~0V
        v = ((static_cast<double>(counts) - 32768.0) * fsVolts) / 32768.0;
    }

    return static_cast<float>(v);
}

static TError ADC_SetScanStartMode()
{
    // throw away one conversion result to avoid 0x80000000 reading from first FIFO read after BRD_Reset()
    out(ofsAdcTriggerOptions, 0);
    out(ofsAdcSoftwareStart, 0);
    return out(ofsAdcTriggerOptions, static_cast<__u8>(0x04)); // "Write 0x04 to +12 // Software Scan Start Mode"
}

static void ADC_DrainFIFO()
{
    // drain any stale FIFO data before starting this scan. // todo? convert to clear fifo operation?
    while (in(ofsAdcFifoCount) > 0)
        (void)in(ofsAdcDataFifo);
}

static void ADC_StartSoftwareADC()
{
    out(ofsAdcSoftwareStart, static_cast<__u8>(0x00)); // 4) Start one scan by writing 0x00 to +16
    usleep(10);
}

static TError ADC_DrainAdcFifoRaw(std::vector<__u32>& raw, size_t expected)
{
    raw.clear();
    raw.reserve(expected);

    const auto t0 = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(2 + expected);

    while (raw.size() < expected)
    {
        const __u32 available = in(ofsAdcFifoCount); // +24 (32-bit)
        if (available == 0)
        {
            if (std::chrono::steady_clock::now() - t0 > timeout)
            {
                // Use whatever your project’s timeout error convention is:
                // return TError::Timeout(...); or return -ETIMEDOUT; etc.
                return static_cast<TError>(-1);
            }
            std::this_thread::yield();
            continue;
        }
        usleep(10);
        __u32 toRead = available;
        while (toRead-- && raw.size() < expected)
        {
            const __u32 v = in(ofsAdcDataFifo); // +1C (32-bit)
            Debug("ADC Raw="+to_hex<__u32>(v));
            if ((v & 0x80000000u) == 0)         // INV bit (discard if set)
                raw.push_back(v);
            else {
                Debug("Discard: "+to_hex<__u32>(v));
            }
        }
    }

    return static_cast<TError>(0);
}

static TError ADC_AcquireScanRaw(std::vector<__u32>& raw, TAdcScanCfg& cfgOut)
{
    out(ofsAdcOversamples, 3);
    cfgOut = ADC_ReadScanCfg();
    if (cfgOut.expected == 0)
        return static_cast<TError>(-1); // invalid config

    TError err = ADC_SetScanStartMode();
    if (err) return err;

    ADC_DrainFIFO();
    ADC_StartSoftwareADC();
    return ADC_DrainAdcFifoRaw(raw, cfgOut.expected);
}

// Same oversample policy as used for volts:
//  - OS=0: use first
//  - OS=1: average both
//  - OS>1: drop first, average the rest
static __u16 ADC_AverageCountsForChannel(const std::vector<__u32>& raw, size_t base, size_t per, __u8 oversamples)
{
    if (oversamples == 0)
    {
        return AdcRawCounts(raw[base]);
    }
    else if (oversamples == 1)
    {
        const __u32 c0 = static_cast<__u32>(AdcRawCounts(raw[base + 0]));
        const __u32 c1 = static_cast<__u32>(AdcRawCounts(raw[base + 1]));
        // Rounded average:
        const __u32 avg = (c0 + c1 + 1u) / 2u;
        return static_cast<__u16>(avg & 0xFFFFu);
    }
    else
    {
        // Drop first, average base+1..base+per-1 (i.e., oversamples samples)
        __u64 sum = 0;
        __u32 n   = 0;

        for (size_t k = 1; k < per; ++k)
        {
            sum += static_cast<__u64>(AdcRawCounts(raw[base + k]));
            ++n;
        }

        const __u64 avg = (sum + (n / 2u)) / n; // rounded
        return static_cast<__u16>(avg & 0xFFFFu);
    }
}

static void ADC_ReduceRawToCountsPerChannel(const std::vector<__u32>& raw, const TAdcScanCfg& cfg, std::vector<__u16>& countsOut)
{
    std::vector<TPerChAccum> acc;
    AccumulateByChannel(raw, cfg, acc);

    countsOut.resize(cfg.nChannels);
    for (size_t i = 0; i < acc.size(); ++i)
        countsOut[i] = FinalizeAvgCounts(acc[i], cfg);
}

static void ADC_ReduceRawToRawPerChannel(const std::vector<__u32>& raw, const TAdcScanCfg& cfg, std::vector<__u32>& rawOut)
{
    std::vector<TPerChAccum> acc;
    AccumulateByChannel(raw, cfg, acc);

    rawOut.resize(cfg.nChannels);
    for (size_t i = 0; i < acc.size(); ++i)
    {
        if (acc[i].meta == 0x80000000u)
        {
            rawOut[i] = 0x80000000u;
            continue;
        }
        const __u16 avgCounts = FinalizeAvgCounts(acc[i], cfg);
        rawOut[i] = acc[i].meta | static_cast<__u32>(avgCounts);
    }
}

static void ADC_ReduceRawToVoltsPerChannel(const std::vector<__u32>& raw, const TAdcScanCfg& cfg, std::vector<float>& voltsOut)
{
    std::vector<TPerChAccum> acc;
    AccumulateByChannel(raw, cfg, acc);

    voltsOut.resize(cfg.nChannels);
    for (size_t i = 0; i < acc.size(); ++i)
    {
        if (acc[i].meta == 0x80000000u)
        {
            voltsOut[i] = std::numeric_limits<float>::quiet_NaN();
            continue;
        }
        const __u16 avgCounts = FinalizeAvgCounts(acc[i], cfg);
        const __u32 cooked = acc[i].meta | static_cast<__u32>(avgCounts);
        voltsOut[i] = AdcRawToVolts(cooked);
    }
}

TADC_VoltsAll::TADC_VoltsAll(DataItemIds dId, const TBytes &data) : TDataItem<ADC_VoltsAllParams>(dId, data)
{
    this->params = {};
}

TDataItemBase& TADC_VoltsAll::Go()
{
    TAdcScanCfg cfg{};
    std::vector<__u32> raw;
    const TError err = ADC_AcquireScanRaw(raw, cfg);

    Debug("ADC_VoltsAll::Go() cfg startCh=" + to_hex<__u32>(cfg.startCh) +
        " endCh=" + to_hex<__u32>(cfg.endCh) +
        " oversamples=" + to_hex<__u32>(cfg.oversamples) +
        " perCh=" + std::to_string(cfg.perChannel) +
        " expected=" + std::to_string(cfg.expected) +
        " got=" + std::to_string(raw.size()));
    if (err)
    {
        Error("ADC_VoltsAll::Go() ADC_AcquireScanRaw failed err=" + std::to_string(err) +
              " got=" + std::to_string(raw.size()) + "/" + std::to_string(cfg.expected));
        this->rawBytes.clear();
        return *this;
    }

    std::vector<float> volts;
    ADC_ReduceRawToVoltsPerChannel(raw, cfg, volts);

    this->rawBytes.resize(volts.size() * sizeof(float));
    std::memcpy(this->rawBytes.data(), volts.data(), this->rawBytes.size());
    std::string mode = "single";
    if (cfg.oversamples == 1)
        mode = "avg2";
    else if (cfg.oversamples > 1)
        mode = "drop1_avg" + std::to_string(cfg.oversamples);

    std::string firstLast;
    if (!volts.empty())
    {
        std::ostringstream oss;
        oss.setf(std::ios::fixed);
        oss << std::setprecision(6) << " first=" << volts.front() << " last=" << volts.back();
        firstLast = oss.str();
    }

    Debug("ADC_VoltsAll::Go() reduce mode=" + mode +
        " nChannels=" + std::to_string(cfg.nChannels) +
        " payloadBytes=" + std::to_string(this->rawBytes.size()) + firstLast);

    return *this;
}


std::string TADC_VoltsAll::AsString(bool bAsReply)
{
    std::string s = "ADC_VoltsAll()";
    if (bAsReply)
    {
        const size_t nFloats = this->rawBytes.size() / sizeof(float);
        s += " -> " + std::to_string(nFloats) + " floats";
    }
    return s;
}


// ---------------- ADC_RawAll ----------------

TADC_RawAll::TADC_RawAll(DataItemIds dId, const TBytes &data)
    : TDataItem<ADC_RawAllParams>(dId, data)
{
}

std::string TADC_RawAll::AsString(bool bAsReply)
{
    std::ostringstream oss;
    oss << "ADC_RawAll()";
    if (bAsReply)
    {
        oss << " -> " << (this->rawBytes.size() / sizeof(__u32)) << " u32";
    }
    return oss.str();
}

TDataItemBase& TADC_RawAll::Go()
{
    TAdcScanCfg cfg{};
    std::vector<__u32> raw;
    const TError err = ADC_AcquireScanRaw(raw, cfg);
    Debug("ADC_RawAll::Go() cfg startCh=" + to_hex<__u32>(cfg.startCh) +
          " endCh=" + to_hex<__u32>(cfg.endCh) +
          " oversamples=" + to_hex<__u32>(cfg.oversamples) +
          " perCh=" + std::to_string(cfg.perChannel) +
          " expected=" + std::to_string(cfg.expected) +
          " got=" + std::to_string(raw.size()));

    if (err)
    {
        Error("ADC_RawAll::Go() ADC_AcquireScanRaw failed err=" + std::to_string(err) +
              " got=" + std::to_string(raw.size()) + "/" + std::to_string(cfg.expected));
        this->rawBytes.clear();
        return *this;
    }

    std::vector<__u32> perChRaw;
    ADC_ReduceRawToRawPerChannel(raw, cfg, perChRaw);

    this->rawBytes.resize(perChRaw.size() * sizeof(__u32));
    std::memcpy(this->rawBytes.data(), perChRaw.data(), this->rawBytes.size());
    std::string firstLast;
    if (!perChRaw.empty())
    {
        firstLast = " first=" + to_hex<__u32>(perChRaw.front()) + " last=" + to_hex<__u32>(perChRaw.back());
    }

    Debug("ADC_RawAll::Go() reduced nChannels=" + std::to_string(cfg.nChannels) +
          " payloadBytes=" + std::to_string(this->rawBytes.size()) + firstLast);

   return *this;
}


// ---------------- ADC_CountsAll ----------------

TADC_CountsAll::TADC_CountsAll(DataItemIds dId, const TBytes &data)
    : TDataItem<ADC_CountsAllParams>(dId, data)
{
}

std::string TADC_CountsAll::AsString(bool bAsReply)
{
    std::ostringstream oss;
    oss << "ADC_CountsAll()";
    if (bAsReply)
    {
        oss << " -> " << (this->rawBytes.size() / sizeof(__u16)) << " u16";
    }
    return oss.str();
}

TDataItemBase& TADC_CountsAll::Go()
{
    TAdcScanCfg cfg{};
    std::vector<__u32> raw;
    const TError err = ADC_AcquireScanRaw(raw, cfg);
    Debug("ADC_CountsAll::Go() cfg startCh=" + to_hex<__u32>(cfg.startCh) +
         " endCh=" + to_hex<__u32>(cfg.endCh) +
         " oversamples=" + to_hex<__u32>(cfg.oversamples) +
         " perCh=" + std::to_string(cfg.perChannel) +
         " expected=" + std::to_string(cfg.expected) +
         " got=" + std::to_string(raw.size()));

    if (err)
    {
        Error("ADC_CountsAll::Go() ADC_AcquireScanRaw failed err=" + std::to_string(err) +
              " got=" + std::to_string(raw.size()) + "/" + std::to_string(cfg.expected));
        this->rawBytes.clear();
        return *this;
    }

    std::vector<__u16> counts;
    ADC_ReduceRawToCountsPerChannel(raw, cfg, counts);

    this->rawBytes.resize(counts.size() * sizeof(__u16));
    std::memcpy(this->rawBytes.data(), counts.data(), this->rawBytes.size());
    std::string firstLast;
    if (!counts.empty())
    {
        firstLast = " first=" + to_hex<__u16>(counts.front()) + " last=" + to_hex<__u16>(counts.back());
    }

    std::string mode = "single";
    if (cfg.oversamples == 1)
        mode = "avg2";
    else if (cfg.oversamples > 1)
        mode = "drop1_avg" + std::to_string(cfg.oversamples);

    Debug("ADC_CountsAll::Go() reduce mode=" + mode +
          " nChannels=" + std::to_string(cfg.nChannels) +
          " payloadBytes=" + std::to_string(this->rawBytes.size()) + firstLast);

    return *this;
}
