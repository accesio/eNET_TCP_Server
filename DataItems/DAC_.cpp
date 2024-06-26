#include "../apci.h"
#include "../logging.h"
#include "../config.h"
#include "../eNET-AIO16-16F.h"
#include "TDataItem.h"
#include "DAC_.h"

TDAC_Output::TDAC_Output(TBytes bytes) : TDataItem(DataItemIds::DAC_Output1, bytes)
{
	Debug("Received: ", bytes);

	if (bytes.size() >= 1)
	{
		GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
		this->dacChannel = bytes[0];
		this->dacCounts = 0x0000;
		this->bWrite = false;
	}
	if (bytes.size() == 3)
	{
		__u16 counts = static_cast<__u16>(bytes[1] | bytes[2] << 8);
		this->bWrite = true;
		// this->dacCounts = counts;  // uncalibrated
		this->dacCounts = static_cast<__u16>(counts * Config.dacScaleCoefficients[this->dacChannel] + Config.dacOffsetCoefficients[this->dacChannel]);
		Debug("DAC "+std::to_string(this->dacChannel)+" output: 0x"+to_hex<__u16>(this->dacCounts));
	}
	return;
}

TBytes TDAC_Output::calcPayload(bool bAsReply)
{
	TBytes bytes;
	stuff<__u8>(bytes, this->dacChannel);
	stuff<__u16>(bytes, this->dacCounts);
	return bytes;
}

std::string TDAC_Output::AsString(bool bAsReply)
{
	return "DAC_Output("+std::to_string(this->dacChannel)+", 0x"+to_hex<__u16>(this->dacCounts)+")";
}

TDAC_Output & TDAC_Output::Go()
{
	__u32 controlValue = 0x00700000 | (this->dacChannel<<16) | this->dacCounts;
	out(ofsDac, controlValue);
	Debug("Wrote " + to_hex<__u32>(controlValue) + " to DAC @ +0x" + to_hex<__u8>(ofsDac));
	return *this;
}

TDAC_Range1::TDAC_Range1(TBytes bytes) : TDataItem(DataItemIds::DAC_Range1, bytes)
{
	Debug("Received: ", bytes);

	if (bytes.size() >= 1)
	{
		GUARD(bytes[0] < 4, ERR_DId_BAD_PARAM, bytes[0]);
		this->dacChannel = bytes[0];
		this->dacRange = 0xFFFFFFFF;
		this->bWrite = false;
	}
	if (bytes.size() == 5)
	{
		__u32 rangeCode = bytes[1] | bytes[2] << 8 | bytes[3] << 16 | bytes[4] << 24;
		if((rangeCode < 4)
		|| (rangeCode==0x30313055)
		|| (rangeCode==0x35303055)
		|| (rangeCode==0x3530E142)
		|| (rangeCode==0x3031E142))
		{
			this->bWrite = true;
			this->dacRange = rangeCode;
			Debug("DAC "+std::to_string(this->dacChannel)+" range set to "+to_hex<__u32>(this->dacRange));
		}
	}
	return;
}

TBytes TDAC_Range1::calcPayload(bool bAsReply)
{
	TBytes bytes;
	stuff<__u8>(bytes, this->dacChannel);
	stuff<__u32>(bytes, this->dacRange);
	return bytes;
}

TDAC_Range1 &TDAC_Range1::Go()
{
	if (this->bWrite)
	{
		// if (permissions(this->permissions, bmConfigFileWrite)) // TODO: something here
		// if (this->dacRange != Config.dacRanges[this->dacChannel])
		{
			// write DAC_Range config file data
			if (0 > WriteConfigString("DAC_RangeCh"+std::to_string(this->dacChannel), to_hex<__u32>(this->dacRange)))
			{
				// handle write error
				this->dacRange = Config.dacRanges[this->dacChannel];
				Error("Failed to write config setting DAC_RangeCh");
			}
			else
				Config.dacRanges[this->dacChannel] = this->dacRange;
			;

			//   ?OR?   issue registered callback into aioenetd so it will
			// if (this->OnWrite != nullptr)
				// this->OnWrite(this->dacChannel, this->dacRange);
		}
	}
	else
	{
		// read DAC_Range1; handled by .AsBytes/.AsString
		// if (this->OnRead != nullptr)
			// this->OnRead(dacChannel);
	}
	return *this;
}

std::string TDAC_Range1::AsString(bool bAsReply)
{
	return "DAC_Range1("+std::to_string(this->dacChannel)+",0x"+to_hex<__u32>(this->dacRange)+")";
}