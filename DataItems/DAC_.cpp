#include <cmath>
#include "DAC_.h"
#include "../apci.h"
#include "../logging.h"
#include "../config.h"
#include "../eNET-AIO16-16F.h"

inline int DacRangeIndexFromCode(__u32 code) {
    switch (code) {
        case RC_UNIPOLAR_5:   case 0: return 0; // U005
        case RC_BIPOLAR_5:    case 1: return 1; // B±05
        case RC_UNIPOLAR_10:  case 2: return 2; // U010
        case RC_BIPOLAR_10:   case 3: return 3; // B±10
        default: return -1;
    }
}

struct DacRangeMap { double span; double offset; };

static constexpr DacRangeMap kDacRangeLut[4] = {
    /* U005  */ {  5.0,   0.0  },
    /* B±05  */ { 10.0,  -0.5  }, // 2*5
    /* U010  */ { 10.0,   0.0  },
    /* B±10  */ { 20.0,  -0.5  }  // 2*10
};

// ====================== TDAC_Output ======================

TDAC_Output::TDAC_Output(const TBytes &bytes)
    : TDataItem(DataItemIds::DAC_Output1, bytes)
{
    Debug("TDAC_Output(const TBytes&) constructor. Received: ", bytes);
    parseBytes(bytes);
}

inline __u16 DACVoltsToCounts(int ch, double Vcal) {
    const int ri = DacRangeIndexFromCode(Config.dacRanges[ch]);
    const double span   = kDacRangeLut[ri].span;
    const double offset = kDacRangeLut[ri].offset;

    double countsF = 65536.0 * (Vcal / span - offset);
    if (countsF < 0.0) countsF = 0.0;
    if (countsF > 65535.0) countsF = 65535.0;
    return static_cast<__u16>(std::lround(countsF));
}

inline __u16 sat_u16_from_double(double v) {
    if (v <= 0.0)      return 0;
    if (v >= 65535.0)  return 0xFFFF;
    return static_cast<__u16>(std::lround(v));
}

void TDAC_Output::parseBytes(const TBytes &bytes)
{
    if (bytes.size() >= 1) {
        GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
        this->params.dacChannel = bytes[0];
        this->bWrite = false;
    }

    const int ch = this->params.dacChannel;
    const double sc = static_cast<double>(Config.dacScaleCoefficients[ch]);   // unitless
    const double oc = static_cast<double>(Config.dacOffsetCoefficients[ch]);  // counts

    if (bytes.size() == 3) {
        // counts path: [iDAC][lo][hi] (little-endian)
        __u16 inCounts = static_cast<__u16>(bytes[1] | (static_cast<__u16>(bytes[2]) << 8));
        this->bWrite = true;

        const double outCountsF = static_cast<double>(inCounts) * sc + oc;
        this->params.dacCounts = sat_u16_from_double(outCountsF);

        Debug("DAC " + std::to_string(ch)
              + " output [counts path]: in=" + to_hex<__u16>(inCounts)
              + " -> out=0x" + to_hex<__u16>(this->params.dacCounts));
    }
    else if (bytes.size() == 5) {
        // volts path: [iDAC][f32 volts]
        float volts;
        std::memcpy(&volts, &bytes[1], sizeof(float));
        this->bWrite = true;

        const __u16 countsNom = DACVoltsToCounts(ch, static_cast<double>(volts));
        const double outCountsF = static_cast<double>(countsNom) * sc + oc;
        this->params.dacCounts = sat_u16_from_double(outCountsF);

        Debug("DAC " + std::to_string(ch)
            + " output [volts path]: V=" + std::to_string(volts)
            + " → nom=0x" + to_hex<__u16>(countsNom)
            + " → out=0x" + to_hex<__u16>(this->params.dacCounts));
    }
    else {
        GUARD(bytes.size() == 1, ERR_DId_BAD_PARAM, bytes.size());
        // size==1: only channel set; no write
    }
}

TBytes TDAC_Output::calcPayload(bool bAsReply)
{
    TBytes out;
    stuff<__u8>(out,  this->params.dacChannel);
    stuff<__u16>(out, this->params.dacCounts);
    return out;
}

std::string TDAC_Output::AsString(bool bAsReply)
{
    return "DAC_Output("
         + std::to_string(this->params.dacChannel)
         + ", 0x" + to_hex<__u16>(this->params.dacCounts)
         + ") " + getDIdDesc();
}

TDAC_Output &TDAC_Output::Go()
{
    __u32 controlValue = 0x00300000
                       | (this->params.dacChannel << 16)
                       | this->params.dacCounts;
    out(ofsDac, controlValue);

    Debug("Wrote " + to_hex<__u32>(controlValue) + " to DAC @ +0x" + to_hex<__u8>(ofsDac));

    return *this;
}

TDAC_OutputV::TDAC_OutputV(const TBytes &bytes)
    : TDataItem(DataItemIds::DAC_Output1V, bytes)
{
    Debug("TDAC_Output(const TBytes&) constructor. Received: ", bytes);
    parseBytes(bytes);
}

void TDAC_OutputV::parseBytes(const TBytes &bytes)
{
    if (bytes.size() >= 1) {
        GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
        this->params.dacChannel = bytes[0];
        this->bWrite = false;
    }

    const int ch = this->params.dacChannel;
    const double sc = static_cast<double>(Config.dacScaleCoefficients[ch]);   // unitless
    const double oc = static_cast<double>(Config.dacOffsetCoefficients[ch]);  // counts

    if (bytes.size() == 3) {
        // counts path: [iDAC][lo][hi] (little-endian)
        __u16 inCounts = static_cast<__u16>(bytes[1] | (static_cast<__u16>(bytes[2]) << 8));
        this->bWrite = true;

        const double outCountsF = static_cast<double>(inCounts) * sc + oc;
        this->params.dacCounts = sat_u16_from_double(outCountsF);

        Debug("DAC " + std::to_string(ch)
              + " output [counts path]: in=" + to_hex<__u16>(inCounts)
              + " -> out=0x" + to_hex<__u16>(this->params.dacCounts));
    }
    else if (bytes.size() == 5) {
        // volts path: [iDAC][f32 volts]
        float volts;
        std::memcpy(&volts, &bytes[1], sizeof(float));
        this->bWrite = true;

        const __u16 countsNom = DACVoltsToCounts(ch, static_cast<double>(volts));
        const double outCountsF = static_cast<double>(countsNom) * sc + oc;
        this->params.dacCounts = sat_u16_from_double(outCountsF);

        Debug("DAC " + std::to_string(ch)
            + " output [volts path]: V=" + std::to_string(volts)
            + " → nom=0x" + to_hex<__u16>(countsNom)
            + " → out=0x" + to_hex<__u16>(this->params.dacCounts));
    }
    else {
        GUARD(bytes.size() == 1, ERR_DId_BAD_PARAM, bytes.size());
        // size==1: only channel set; no write
    }
}

TBytes TDAC_OutputV::calcPayload(bool bAsReply)
{
    TBytes out;
    stuff<__u8>(out,  this->params.dacChannel);
    stuff<float_t>(out, this->params.dacCounts);
    return out;
}

std::string TDAC_OutputV::AsString(bool bAsReply)
{
    return "DAC_Output("
         + std::to_string(this->params.dacChannel)
         + ", 0x" + to_hex<__u16>(this->params.dacCounts)
         + ") " + getDIdDesc();
}

TDAC_OutputV &TDAC_OutputV::Go()
{
    __u32 controlValue = 0x00300000
                       | (this->params.dacChannel << 16)
                       | this->params.dacCounts;
    out(ofsDac, controlValue);

    Debug("Wrote " + to_hex<__u32>(controlValue) + " to DAC @ +0x" + to_hex<__u8>(ofsDac));

    return *this;
}

// ====================== TDAC_Range1 ======================

TDAC_Range1::TDAC_Range1(const TBytes &bytes)
    : TDataItem(DataItemIds::DAC_Range1, bytes)
{
    Debug("TDAC_Range1(const TBytes&) constructor. Received: ", bytes);
    parseBytes(bytes);
}

void TDAC_Range1::parseBytes(const TBytes &bytes)
{
    if (bytes.size() >= 1)
    {
        GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
        this->params.dacChannel = bytes[0];
        this->params.dacRange   = 0xFFFFFFFF;
        this->bWrite = false;
    }
    if (bytes.size() == 5)
    {
        __u32 rangeCode = (bytes[1])
                        | (bytes[2] << 8)
                        | (bytes[3] << 16)
                        | (bytes[4] << 24);
        if ((rangeCode < 4)
         || (rangeCode == 0x30313055)
         || (rangeCode == 0x35303055)
         || (rangeCode == 0x3530E142)
         || (rangeCode == 0x3031E142))
        {
            this->bWrite = true;
            this->params.dacRange = rangeCode;

            Debug("DAC " + std::to_string(this->params.dacChannel)
                  + " range set to 0x" + to_hex<__u32>(this->params.dacRange));
        }
    }
}

TBytes TDAC_Range1::calcPayload(bool bAsReply)
{
    TBytes out;
    stuff<__u8>(out,  this->params.dacChannel);
    stuff<__u32>(out, this->params.dacRange);
    return out;
}

TDAC_Range1 &TDAC_Range1::Go()
{
    if (this->bWrite)
    {
        // if (this->permissionsCheck?) // placeholder
        if (0 > WriteConfigString("DAC_RangeCh" + std::to_string(this->params.dacChannel), to_hex<__u32>(this->params.dacRange)))
        {
            this->params.dacRange = Config.dacRanges[this->params.dacChannel];
            Error("Failed to write config setting DAC_RangeCh");
        }
        else
        {
            Config.dacRanges[this->params.dacChannel] = this->params.dacRange;
        }
    }
    else
    {
        // read scenario, no special action needed
    }
    return *this;
}

std::string TDAC_Range1::AsString(bool bAsReply)
{
    return "DAC_Range1("
         + std::to_string(this->params.dacChannel)
         + ",0x" + to_hex<__u32>(this->params.dacRange)
         + ")";
}
