#include "DAC_.h"
#include "../apci.h"
#include "../logging.h"
#include "../config.h"
#include "../eNET-AIO16-16F.h"

// ====================== TDAC_Output ======================

TDAC_Output::TDAC_Output(const TBytes &bytes)
    : TDataItem(DataItemIds::DAC_Output1, bytes)
{
    Debug("TDAC_Output(const TBytes&) constructor. Received: ", bytes);
    parseBytes(bytes);
}

void TDAC_Output::parseBytes(const TBytes &bytes)
{
    // Keep your original partial parse logic:
    if (bytes.size() >= 1)
    {
        GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
        this->params.dacChannel = bytes[0];
        this->bWrite = false;
    }
    if (bytes.size() == 3)
    {
        __u16 counts = static_cast<__u16>(bytes[1] | (bytes[2] << 8));
        this->bWrite = true;
        this->params.dacCounts = static_cast<__u16>(
            counts * Config.dacScaleCoefficients[this->params.dacChannel]
            + Config.dacOffsetCoefficients[this->params.dacChannel]
        );

        Debug("DAC " + std::to_string(this->params.dacChannel)
              + " output: 0x" + to_hex<__u16>(this->params.dacCounts));
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
    __u32 controlValue = 0x00700000
                       | (this->params.dacChannel << 16)
                       | this->params.dacCounts;
    out(ofsDac, controlValue);

    Debug("Wrote " + to_hex<__u32>(controlValue)
          + " to DAC @ +0x" + to_hex<__u8>(ofsDac));

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
