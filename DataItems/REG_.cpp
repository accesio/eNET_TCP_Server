#include "REG_.h"
#include "../apci.h"
#include "../eNET-AIO16-16F.h"
#include "../logging.h"
#include "TDataItem.h"


// TODO: write WaitUntilBitsMatch(__u8 offset, __u32 bmMask, __u32 bmPattern);
int WaitUntilRegisterBitIsLow(__u8 offset, __u32 bitMask) // TODO: move into utility source file
{
	__u32 value = 0;
	int attempt = 0;
	do
	{
		value = in(offset);
		Trace("SPI Busy Bit at " + std::string(to_hex<__u8>(offset)) + " is " + ((value & bitMask) ? "1" : "0"));
		// if (status < 0)
		// 	return -errno;
		if (++attempt > 100)
		{
			Error("Timeout waiting for SPI to be not busy, at offset: " + to_hex<__u8>(offset));
			return -ETIMEDOUT; // TODO: swap "attempt" with "timeout" RTC if benchmark proves RTC is not too slow
		}
	} while ((value & bitMask));

	return 0;
}

// ======================== TREG_Read1 ========================

TREG_Read1 &TREG_Read1::Go()
{
    // read hardware
    this->params.regVal = in(this->params.offset);
    return *this;
}

std::shared_ptr<void> TREG_Read1::getResultValue()
{
    Trace("returning "
          + (this->params.width == 8
             ? to_hex<__u8>((__u8)this->params.regVal)
             : to_hex<__u32>((__u32)this->params.regVal)));

    if (this->params.width == 8)
    {
        return std::make_shared<__u8>((__u8)this->params.regVal);
    }
    else
    {
        return std::make_shared<__u32>((__u32)this->params.regVal);
    }
}

TError TREG_Read1::validateDataItemPayload(DataItemIds DataItemID, TBytes Data)
{
    TError result = ERR_SUCCESS;
    if (Data.size() != 1)
    {
        Error(err_msg[-ERR_DId_BAD_PARAM]);
        return ERR_DId_BAD_PARAM;
    }
    int offset = Data[0];
    int w = widthFromOffset(offset);
    if (w != 0)
    {
        Error(err_msg[-ERR_DId_BAD_OFFSET]);
        return ERR_DId_BAD_OFFSET;
    }
    return result;
}

TREG_Read1::TREG_Read1(DataItemIds DId, int ofs)
    : TDataItem<REG_Read1Params>(DId, {})
{
    this->setOffset(ofs);
}

TREG_Read1::TREG_Read1()
    : TDataItem<REG_Read1Params>(DataItemIds::REG_Read1, {})
{
}

TREG_Read1::TREG_Read1(const TBytes &data)
    : TDataItem<REG_Read1Params>(DataItemIds::REG_Read1, data)
{
    Trace("ENTER. TBytes: ", data);

    GUARD(data.size() == 1, ERR_DId_BAD_PARAM, static_cast<int>(data.size()));
    this->params.offset = data[0];
    int w = widthFromOffset(this->params.offset);
    GUARD(w != 0, ERR_DId_BAD_PARAM, static_cast<int>(DataItemIds::REG_Read1));
    this->params.width = w;
}

TREG_Read1::TREG_Read1(DataItemIds DId, const TBytes &FromBytes)
    : TDataItem<REG_Read1Params>(DId, FromBytes)
{
    if (!FromBytes.empty())
    {
        this->params.offset = FromBytes[0];
        this->params.width  = widthFromOffset(FromBytes[0]);
    }
}

TREG_Read1 &TREG_Read1::setOffset(int ofs)
{
    int w = widthFromOffset(ofs);
    if (w == 0)
    {
        Error("Invalid offset");
        throw std::logic_error("Invalid offset passed to TREG_Read1::setOffset");
    }
    this->params.offset = ofs;
    this->params.width  = w;
    return *this;
}

TBytes TREG_Read1::calcPayload(bool bAsReply)
{
    TBytes bytes;
    // store offset
    bytes.push_back(static_cast<__u8>(this->params.offset));
    Debug("offset = " + to_hex<__u8>((__u8)this->params.offset) + " bytes now holds: ", bytes);

    if (bAsReply)
    {
        // read the result
        auto value = this->getResultValue();
        __u32 v = *((__u32 *)value.get());

        if (this->params.width == 8)
        {
            bytes.push_back(static_cast<__u8>(v & 0xFF));
            Debug("byte reg value=" + to_hex<__u8>(v & 0xff), bytes);
        }
        else
        {
            Debug("byte reg value=" + to_hex<__u32>(v), bytes);
            stuff<__u32>(bytes, v);
        }
    }
    Trace("TREG_Read1::calcPayload built: ", bytes);
    return bytes;
}

std::string TREG_Read1::AsString(bool bAsReply)
{
    std::stringstream dest;
    dest << "REG_Read1("
         << std::hex << std::setw(2) << std::setfill('0')
         << this->params.offset << ")";

    if (bAsReply)
    {
        dest << " → ";
        auto value = this->getResultValue();
        __u32 v = *((__u32 *)value.get());
        if (this->params.width == 8)
        {
            dest << std::hex << std::setw(2) << (v & 0xFF);
        }
        else
        {
            dest << std::hex << std::setw(8) << v;
        }
    }
    Trace("Built: " + dest.str());
    return dest.str();
}

// ======================== TREG_Writes ========================

TREG_Writes::TREG_Writes(DataItemIds DId)
    : TDataItem<REG_WritesParams>(DId, {})
{
}

TREG_Writes::TREG_Writes()
    : TDataItem<REG_WritesParams>(DataItemIds::INVALID, {})
{
}

TREG_Writes::~TREG_Writes()
{
    Trace("Attempting to free the memory in this->params.Writes vector");
    this->params.Writes.clear();
}

TREG_Writes::TREG_Writes(const TBytes &buf)
    : TDataItem<REG_WritesParams>(DataItemIds::INVALID, buf)
{
    Debug("CHAINING1");
}

TREG_Writes::TREG_Writes(DataItemIds DId, const TBytes &FromBytes)
    : TDataItem<REG_WritesParams>(DId, FromBytes)
{
    Debug("CHAINING2");
}

TREG_Writes &TREG_Writes::Go()
{
    this->resultCode = 0;
    for (auto &action : this->params.Writes)
    {
        // e.g., DAC / DIO SPI busy checks
        switch (action.offset)
        {
        case ofsDac:
            this->resultCode = WaitUntilRegisterBitIsLow(ofsDacSpiBusy, bmDacSpiBusy);
            break;
        case ofsDioDirections:
        case ofsDioOutputs:
        case ofsDioInputs:
            this->resultCode = WaitUntilRegisterBitIsLow(ofsDioSpiBusy, bmDioSpiBusy);
            break;
        default:
            break;
        }
        out(action.offset, action.value);

        if (action.width == 8)
        {
            Trace("out(" + to_hex<__u8>(action.offset) + ") → " + to_hex<__u8>(action.value));
        }
        else
        {
            Trace("out(" + to_hex<__u8>(action.offset) + ") → " + to_hex<__u32>(action.value));
        }
    }
    return *this;
}

TREG_Writes &TREG_Writes::addWrite(__u8 w, int ofs, __u32 value)
{
    Trace("ENTER, w:" + std::to_string(w)
          + ", offset: " + to_hex<__u8>(ofs)
          + ", value: " + to_hex<__u32>(value));
    REG_Write aWrite;
    aWrite.width  = w;
    aWrite.offset = static_cast<__u8>(ofs);
    aWrite.value  = value;
    this->params.Writes.emplace_back(aWrite);
    return *this;
}

std::string TREG_Writes::AsString(bool bAsReply)
{
    std::stringstream dest;
    for (auto &aWrite : this->params.Writes)
    {
        __u32 v = aWrite.value;
        dest << "REG_Write1("
             << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(aWrite.offset) << ", "
             << std::hex << std::setw(aWrite.width / 8 * 2) << v
             << "); ";
    }
    Debug(dest.str());
    return dest.str();
}

// ======================== TREG_Write1 ========================

TREG_Write1::~TREG_Write1()
{
    this->params.Writes.clear();
}

TREG_Write1::TREG_Write1(DataItemIds ID, const TBytes &buf)
    : TREG_Writes(DataItemIds::REG_Write1, buf)
{
    GUARD(buf.size() > 0, ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH, 0);
    __u8 ofs = buf[0];
    int w = widthFromOffset(ofs);
    GUARD(w != 0, ERR_DId_BAD_OFFSET, ofs);
    GUARD(w == 8 ? (buf.size() == 2) : (buf.size() == 5),
          ERR_DId_BAD_PARAM,
          static_cast<int>(buf.size()));

    __u32 value = 0;
    if (w == 8)
        value = buf[1];
    else
        value = *(__u32 *)&buf[1];

    this->addWrite(static_cast<__u8>(w), ofs, value);
    Trace("width=" + std::to_string(w) + " value=" + to_hex<__u32>(value));
}

TBytes TREG_Write1::calcPayload(bool bAsReply)
{
    TBytes bytes;
    if (!this->params.Writes.empty())
    {
        auto &w0 = this->params.Writes[0];
        stuff<__u8>(bytes, w0.offset);

        __u32 v = w0.value;
        for (int i = 0; i < w0.width / 8; i++)
        {
            bytes.push_back(v & 0xFF);
            v >>= 8;
        }
    }
    else
    {
        Error("ERROR: nothing in Write[] queue");
    }
    return bytes;
}
