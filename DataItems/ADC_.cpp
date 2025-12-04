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
    // stuff() your param into bytes
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

// -------------------- TADC_StreamStart --------------------

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

// -------------------- TADC_StreamStop --------------------
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
// --------------------------------------------------------------------------
// TADC_Differential1 Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_DifferentialAll Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_Range1 Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_RangeAll Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_Span1 Implementation (Scale calibration for one range)
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_ScaleAll Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_Offset1 Implementation (Offset calibration for one range)
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_OffsetAll Implementation
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_Calibration1 Implementation (both scale and offset for one range)
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// TADC_CalibrationAll Implementation
// --------------------------------------------------------------------------

// --------------------------------------------------------------------------
// TADC_CalibrationAll Constructor (from interleaved TBytes)
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// Go(): Write calibration values for all 8 ranges
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// calcPayload(): Return the interleaved scale/offset pairs as bytes
// --------------------------------------------------------------------------
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

// --------------------------------------------------------------------------
// AsString(): Provide an explicit string representation per range
// --------------------------------------------------------------------------
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

// #include "ADC_.h"
// #include "../apci.h"
// #include "../logging.h"
// #include "../eNET-AIO16-16F.h"
// #include "../adc.h"

// extern int apci;

// TADC_BaseClock::TADC_BaseClock(TBytes buf)
// {
// 	this->setDId(ADC_BaseClock);
// 	GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);
// }

// TBytes TADC_BaseClock::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	stuff(bytes, this->baseClock);
// 	Trace("TADC_BaseClock::calcPayload built: ", bytes);
// 	return bytes;
// };

// TADC_BaseClock &TADC_BaseClock::Go()
// {
// 	Trace("ADC_BaseClock Go()");
// 	this->baseClock = in(ofsAdcBaseClock);
// 	return *this;
// }

// std::string TADC_BaseClock::AsString(bool bAsReply)
// {
// 	std::stringstream dest;

// 	dest << "ADC_BaseClock()";
// 	if (bAsReply)
// 	{
// 		dest << " → " << this->baseClock;
// 	}
// 	Trace("Built: " + dest.str());
// 	return dest.str();
// }

// TADC_StreamStart::TADC_StreamStart(TBytes buf)
// {
// 	this->setDId(ADC_StreamStart);
// 	GUARD((buf.size() == 0) || (buf.size() == 4), ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);

// 	if (buf.size() == 4)
// 	{
// 		if (-1 == AdcStreamingConnection)
// 		{
// 			this->argConnectionID = (int)*(__u32 *)buf.data();
// 			AdcStreamingConnection = this->argConnectionID;
// 		}
// 		else
// 		{
// 			Error("ADC Busy");
// 			throw std::logic_error("ADC Busy already, on Connection: "+std::to_string(AdcStreamingConnection));
// 		}
// 	}
// 	Trace("AdcStreamingConnection: "+std::to_string(AdcStreamingConnection));
// }

// TBytes TADC_StreamStart::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	stuff(bytes, this->argConnectionID);
// 	Trace("TADC_StreamStart::calcPayload built: ", bytes);
// 	return bytes;
// };

// TADC_StreamStart &TADC_StreamStart::Go()
// {
// 	Trace("ADC_StreamStart::Go(), ADC Streaming Data will be sent on ConnectionID: "+std::to_string(AdcStreamingConnection));
// 	auto status = apciDmaTransferSize(RING_BUFFER_SLOTS, BYTES_PER_TRANSFER);
// 	if (status)
// 	{
// 		Error("Error setting apciDmaTransferSize: "+std::to_string(status));
// 		throw std::logic_error(err_msg[-status]);
// 	}

// 	AdcStreamTerminate = 0;
// 	if (AdcWorkerThreadID == -1)
// 	{
// 		AdcWorkerThreadID = pthread_create(&worker_thread, NULL, &worker_main, &AdcStreamingConnection);
// 	}
// 	apciDmaStart();
// 	return *this;
// };

// std::string TADC_StreamStart::AsString(bool bAsReply)
// {
// 	std::string msg = this->getDIdDesc();
// 	if (bAsReply)
// 	{
// 		msg += ", ConnectionID = " + to_hex<int>(this->argConnectionID);
// 	}
// 	return msg;
// };

// TADC_StreamStop::TADC_StreamStop(TBytes buf)
// {
// 	this->setDId(ADC_StreamStop);
// 	GUARD(buf.size() == 0, ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);
// }

// TBytes TADC_StreamStop::calcPayload(bool bAsReply)
// {
// 	TBytes bytes;
// 	return bytes;
// };

// TADC_StreamStop &TADC_StreamStop::Go()
// {
// 	Trace("ADC_StreamStop::Go(): terminating ADC Streaming");
// 	AdcStreamTerminate = 1;
// 	apciCancelWaitForIRQ();
// 	AdcStreamingConnection = -1;
// 	AdcWorkerThreadID = -1;
// 	// pthread_cancel(worker_thread);
// 	// pthread_join(worker_thread, NULL);
// 	Trace("ADC_StreamStop::Go() exiting");
// 	return *this;
// };
