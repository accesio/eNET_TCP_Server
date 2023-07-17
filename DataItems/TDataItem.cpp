#include <tuple>
#include <iostream>
#include <map>
#include <iterator>
#include <algorithm>
#include <cmath>

#include "../logging.h"
#include "../utilities.h"
#include "TDataItem.h"
#include "ADC_.h"
#include "BRD_.h"
#include "CFG_.h"
#include "DAC_.h"
#include "REG_.h"
#include "SYS_.h"
#include "../eNET-AIO16-16F.h"

#define DIdNYI(d)                                         \
	{                                                     \
		d,                                                \
		{                                                 \
			0, 0, 0, construct<TDataItemNYI>, #d " (NYI)" \
		}                                                 \
	}

#define DATA_ITEM_IMPL_2(x, aclass, a, b, c, y, z)                                                 \
	{                                                                                              \
		x,                                                                                         \
		{                                                                                          \
			a, b, c, [](DataItemIds q, TBytes bytes) { return construct<aclass>(x, bytes); }, y, z \
		}                                                                                          \
	}
#define DATA_ITEM_IMPL_1(x, aclass, a, b, c, y)                                                 \
	{                                                                                           \
		x,                                                                                      \
		{                                                                                       \
			a, b, c, [](DataItemIds q, TBytes bytes) { return construct<aclass>(x, bytes); }, y \
		}                                                                                       \
	}
#define DATA_ITEM_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define DATA_ITEM(...) DATA_ITEM_GET_MACRO(__VA_ARGS__, DATA_ITEM_IMPL_2, DATA_ITEM_IMPL_1)(__VA_ARGS__)


extern bool done;
const std::map<DataItemIds, TDIdDictEntry> DIdDict =
	{
		//{INVALID, { 9, 9, 9,( [](DataItemIds x, TBytes bytes) { return construct<TDataItem>(x, bytes); }), "DAC_Calibrate1(u8 iDAC, single Offset, single Scale)", nullptr}},
		DATA_ITEM(DataItemIds::INVALID, TDataItem, 0, 0, 0, "Invalid DId -1", nullptr),
		DATA_ITEM(DataItemIds::BRD_, TDataItem, 0, 0, 0, "Invalid DID 0", nullptr),
		DATA_ITEM(DataItemIds::BRD_Reset, TDataItem, 0, 0, 0, "BRD_Reset()", nullptr),
		DATA_ITEM(DataItemIds::BRD_DeviceID, TBRD_DeviceID, 0, 0, 0, "BRD_DeviceID() → u32", nullptr),
		DATA_ITEM(DataItemIds::BRD_Features, TBRD_Features, 0, 4, 255, "BRD_Features() → u8", nullptr),
		DATA_ITEM(DataItemIds::BRD_FpgaID, TBRD_FpgaId, 0, 4, 255, "BRD_FpgaID() → u32", nullptr),
		DATA_ITEM(DataItemIds::BRD_REBOOT, TDataItem, 8, 8, 8, "BRD_REBOOT(double)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
		 			double token = *(double *)args;
		 			if (token == M_PI)
		 				done = true; })),
		DATA_ITEM(DataItemIds::REG_Read1, TREG_Read1, 1, 1, 1, "REG_Read1(u8 offset) → [u8|u32]", nullptr),
		// DIdNYI(REG_ReadBuf),
		DATA_ITEM(DataItemIds::REG_Write1, TREG_Write1, 2, 5, 5, "REG_Write1(u8 ofs, [u8|u32] data)", nullptr),
		// DIdNYI(REG_WriteBuf),

		DATA_ITEM(DataItemIds::REG_ClearBits, TDataItem, 2, 5, 5,
				  "REG_ClearBits(u8 ofs, u8|u32 bitsToClear)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
						__u8 * pargs = (__u8 *)args;
						__u8 ofs = *pargs;
						pargs++;
						__u32 data = 0;
						__u32 bits = 0;
						bits = regextract(pargs, ofs);

						data = in(ofs);
						data &= ~ bits;
						out(ofs,data); })),
		DATA_ITEM(DataItemIds::REG_ToggleBits, TDataItem, 2, 5, 5,
				  "REG_SetBits(u8 ofs, u8|u32 bitsToToggle)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
				__u8 * pargs = (__u8 *)args;
				__u8 ofs = *pargs;
				pargs++;
				__u32 data = 0;
				__u32 bits = 0;
				bits = regextract(pargs, ofs);
				data = in(ofs);
				data ^= bits;
				out(ofs,data); })),
		DATA_ITEM(DataItemIds::REG_SetBits, TDataItem, 2, 5, 5, "REG_SetBits(u8 ofs, u8|u32 bitsToSet)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
				__u8 * pargs = (__u8 *)args;
				__u8 ofs = *((__u8 *)args) ;
				pargs++;
				__u32 bits = 0;
				bits = regextract(pargs, ofs);
				__u32 data = 0;
				// switch(widthFromOffset(ofs))
				// {
				// 	case 8: bits = *(__u8 *)(pargs);
				// 	break;
				// 	case 16:bits = *(__u16 *)(pargs);
				// 	break;
				// 	case 32:bits = *(__u32 *)(pargs);
				// 	break;
				// }
				data = in(ofs);
				data |= bits;
				out(ofs,data); })),

		// {DAC_, {0, 0, 0, construct<TDataItem>, "TDataItemBase (DAC_)"}},
		DATA_ITEM(DataItemIds::DAC_Output1, TDAC_Output, 5, 5, 5, "DAC_Output1(u8 iDAC, single Volts)", nullptr),
		DATA_ITEM(DataItemIds::DAC_Range1, TDAC_Range1, 5, 5, 5, "DAC_Range1(u8 iDAC, u32 RangeCode)", nullptr),
		// DIdNYI(DAC_Configure1),
		// DIdNYI(DAC_ConfigAndOutput1),
		DATA_ITEM(DataItemIds::DAC_Calibrate1, TDataItem, 9, 9, 9, "DAC_Calibrate1(u8 iDAC, single Offset, single Scale)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
					float offset;
					float scale;
					__u8 dacnum;
					dacnum = *(((__u8 *)args) + 0);
					offset = *(float *)(((__u8 *)args) + 5);
					scale = *(float *)(((__u8 *)args) + 1);
					printf("....DAC %hhx scale=%3.3f offset=%3.3f\n", dacnum, scale, offset);
					Config.dacScaleCoefficients[dacnum] = scale;
					Config.dacOffsetCoefficients[dacnum] = offset;
					printf("....Config.scale[%hhx]=%3.3f Config.offset[%hhx]=%3.3f\n", dacnum, Config.dacScaleCoefficients[dacnum], dacnum, Config.dacOffsetCoefficients[dacnum]);
					Debug("inside lambda: DAC " + std::to_string(dacnum)) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]); })),

		DATA_ITEM(DataItemIds::DAC_Offset1, TDataItem, 5, 5, 5, "DAC_Offset1(u8 iDac, single Offset)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
		 			float offset;
		 			__u8 dacnum;
		 			dacnum = *(((__u8 *)args) + 0);
		 			offset = *(float *)(((__u8 *)args) + 1);
		 			printf("....DAC %hhx offset=%3.3f\n", dacnum, offset);
		 			Config.dacOffsetCoefficients[dacnum] = offset;
		 			printf("....Config.offset[%hhx]=%3.3f\n", dacnum, Config.dacOffsetCoefficients[dacnum]);
		 			Debug("inside lambda: DAC " + std::to_string(dacnum)) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]); })),

		DATA_ITEM(DataItemIds::DAC_OffsetAll, TDataItem, 16, 16, 16, "DAC_OffsetAll(single offset0, offset1, offset2, offset3)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
					for (__u8 dacnum=0; dacnum < 4; ++dacnum)
					{
					float offset = *(float *)(((__u8 *)args) + dacnum*4);
					printf("....DAC %hhx offset=%3.3f\n", dacnum, offset);
					Config.dacOffsetCoefficients[dacnum] = offset;
					printf("....Config.offset[%hhx]=%3.3f\n", dacnum, Config.dacOffsetCoefficients[dacnum]);
					Debug("inside lambda: DAC " + std::to_string(dacnum)) + ", offset = " + std::to_string(Config.dacOffsetCoefficients[dacnum]);
					} })),
		DATA_ITEM(DataItemIds::DAC_Scale1, TDataItem, 5, 5, 5, "DAC_Scale1(u8 iDac, single Scale)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
					float scale;
					__u8 dacnum;
					dacnum = *(((__u8 *)args) + 0);
					scale = *(float *)(((__u8 *)args) + 1);
					printf("....DAC %hhx scale=%3.3f \n", dacnum, scale);
					Config.dacScaleCoefficients[dacnum] = scale;
					printf("....Config.scale[%hhx]=%3.3f \n", dacnum, Config.dacScaleCoefficients[dacnum]);
					Debug("inside lambda: DAC " + std::to_string(dacnum)) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]); })),

		DATA_ITEM(DataItemIds::DAC_ScaleAll, TDataItem, 16, 16, 16, "DAC_OffsetAll(single scale0, scale1, scale2, scale3)",
				  static_cast<std::function<void(void *)>>([](void *args)
														   {
					for (__u8 dacnum=0;dacnum<4;++dacnum)
					{
					float scale = *(float *)(((__u8 *)args) + dacnum*4);
					printf("....DAC %hhx scale=%3.3f\n", dacnum, scale);
					Config.dacScaleCoefficients[dacnum] = scale;
					printf("....Config.scale[%hhx]=%3.3f\n", dacnum, Config.dacScaleCoefficients[dacnum]);
					Debug("inside lambda: DAC " + std::to_string(dacnum)) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]);
					} })),

		// DIdNYI(DIO_),
		// DIdNYI(DIO_Configure1),
		// DIdNYI(DIO_Input1),
		// DIdNYI(DIO_InputBuf1),
		// DIdNYI(DIO_Output1),
		// DIdNYI(DIO_OutputBuf),
		// DIdNYI(DIO_ConfigureReadWriteReadSome),
		// DIdNYI(DIO_Clear1),
		// DIdNYI(DIO_Set1),
		// DIdNYI(DIO_Toggle1),
		// DIdNYI(DIO_Pulse1),

		// DIdNYI(PWM_),
		// DIdNYI(PWM_Configure1),
		// DIdNYI(PWM_Input1),
		// DIdNYI(PWM_Output1),

		// DIdNYI(ADC_),
		// DIdNYI(ADC_Claim),
		// DIdNYI(ADC_Release),
		DATA_ITEM(DataItemIds::ADC_BaseClock, TADC_BaseClock, 0, 0, 4, "ADC_BaseClock() → u32", nullptr),
		DATA_ITEM(DataItemIds::ADC_StartHz, TDataItem, 4, 4, 4, "ADC_StartHz(f32)", nullptr),
		DATA_ITEM(DataItemIds::ADC_StartDivisor, TDataItem, 4, 4, 4, "ADC_StartDivisor(u32)", nullptr),
		// DIdNYI(ADC_ConfigurationOfEverything),
		// DIdNYI(ADC_Differential1),
		// DIdNYI(ADC_DifferentialAll),
		// DIdNYI(ADC_Range1),
		// DIdNYI(ADC_RangeAll),
		// DIdNYI(ADC_Span1),
		// DIdNYI(ADC_SpanAll),
		// DIdNYI(ADC_Offset1),
		// DIdNYI(ADC_OffsetAll),
		// DIdNYI(ADC_Calibration1),
		// DIdNYI(ADC_CalibrationAll),
		// DIdNYI(ADC_Volts1),
		// DIdNYI(ADC_VoltsAll), // ADC_GetScanV
		// DIdNYI(ADC_Counts1),
		// DIdNYI(ADC_CountsAll), // ADC_GetScanCounts
		// DIdNYI(ADC_Raw1),
		// DIdNYI(ADC_RawAll),   // ADC_GetScanRaw
		DATA_ITEM(DataItemIds::ADC_StreamStart, TDataItem, 4, 4, 4, "ADC_StreamStart((u32)AdcConnectionId)", nullptr),
		DATA_ITEM(DataItemIds::ADC_StreamStop, TDataItem, 0, 0, 0, "ADC_StreamStop()", nullptr),

		// DIdNYI(ADC_Streaming_stuff_including_Hz_config),

		// DIdNYI(SCRIPT_Pause), // SCRIPT_Pause(__u8 delay ms)
		// DIdNYI(WDG_),
		// DIdNYI(DEF_),
		// DIdNYI(SERVICE_),
		// DIdNYI(TCP_),
		DATA_ITEM(DataItemIds::TCP_ConnectionID, TDataItem, 0, 0, 255, "TCP_ConnectionID() → u32", nullptr),
		// DIdNYI(PNP_),
		// DIdNYI(CFG_),
		DATA_ITEM(DataItemIds::CFG_Hostname, TDataItem, 1, 20, 253, "CFG_Hostname({valid Hostname})", nullptr),
		DATA_ITEM(DataItemIds::SYS_UploadFileName, TSYS_UploadFileName, 1, 255, 255, "SYS_UploadFileName({valid filepath})", nullptr),
		DATA_ITEM(DataItemIds::SYS_UploadFileData, TSYS_UploadFileData, 1, 65534, 65534, "SYS_UploadFileData({valid file data})", nullptr),
/*
 */
};

#pragma region TDataItem implementation
/*	TDataItem
	Base Class, provides basics for handling TDataItem payloads.
	Descendant classes should handle stuff specific to the TDataItem's DataItemId ("DId")
	e.g, if the Message is equivalent to ADC_SetRangeAll(__u8 ranges[16]) then the Data
		should be 16 bytes, each of which is a valid Range Code
	e.g, if the Message is ADC_SetRange(__u16 Channel, __u8 RangeCode) then the Data
		should be 3 bytes, being a __u16 Channel which must be valid for this device, and
		a valid RangeCode byte
	NOTE:
		I suspect I need more ABCs: TWriteableData() and TReadOnlyData(), perhaps...
*/
#pragma region TDataItem Class Methods(static)
// NYI - validate the payload of a Data Item based on the Data Item ID
// e.g, if the Message is equivalent to ADC_SetRangeAll(__u8 ranges[16]) then the Data
//	  should be 16 bytes, each of which is a valid Range Code
// e.g, if the Message is ADC_SetRange(__u16 Channel, __u8 RangeCode) then the Data
//	  should be 3 bytes, being a __u16 Channel which must be valid for this device, and
//	  a valid RangeCode byte
// NOTE:
//   This should be implemented OOP-style: each TDataItem ID should be a descendant-class that provides the
//   specific validate and parse appropriate to that DataItemID
int TDataItem::validateDataItemPayload(DataItemIds DId, TBytes bytes)
{
	Trace("ENTER, DId: " + to_hex<TDataId>(DId) + ": ", bytes);
	int result = ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH;
	TDataItemLength len = bytes.size();
	Trace(std::to_string(getMinLength(DId)) + " <= " + std::to_string(len) + " <= " + std::to_string(getMaxLength(DId)));
	if ((TDataItem::getMinLength(DId) <= len) && (len <= TDataItem::getMaxLength(DId)))
	{
		result = ERR_SUCCESS;
		Trace("Valid");
	}
	else
	{
		Error("INVALID");
	}
	return result;
}

int TDataItem::getDIdIndex(DataItemIds DId)
{
	auto item = DIdDict.find(DId);
	if (item != DIdDict.end())
	{
		return std::distance(DIdDict.begin(), item);
	}
	throw std::logic_error("DId not found in list");
}

// returns human-readable description of this TDataItem
std::string TDataItem::getDIdDesc(DataItemIds DId)
{
	return DIdDict.find(DId)->second.desc;
}

TDataItemLength TDataItem::getMinLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.minLen;
}

TDataItemLength TDataItem::getTargetLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.maxLen;
}

TDataItemLength TDataItem::getMaxLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.maxLen;
}

int TDataItem::isValidDataItemID(DataItemIds DId)
{
	Debug("DataItemId: " + to_hex<__u16>((static_cast<__u16>(DId))));
	auto item = DIdDict.find(DId);
	return (item != DIdDict.end());
}

int TDataItem::validateDataItem(TBytes bytes)
{
	int result = 0;
	if (bytes.size() < sizeof(TDataItemHeader))
	{
		Error(err_msg[-ERR_MSG_DATAITEM_TOO_SHORT]);
		return ERR_MSG_DATAITEM_TOO_SHORT;
	}
	TDataItemHeader *head = (TDataItemHeader *)bytes.data();

	TDataItemLength DataItemSize = head->dataLength; // WARN: only works if TDataItemHeader length field is one byte
	DataItemIds Id = head->DId;
	if (!isValidDataItemID(Id))
	{
		Error(err_msg[ERR_MSG_DATAITEM_ID_UNKNOWN]);
		return ERR_MSG_DATAITEM_ID_UNKNOWN;
	}

	if (DataItemSize == 0)
	{ // no data in this data item so no need to check the payload
		Trace("validateDataItem status: " + std::to_string(result) + ", " + err_msg[-result]);
		return result;
	}
	else
	{
		TBytes Data = bytes;
		Data.erase(Data.cbegin(), Data.cbegin() + sizeof(TDataItemHeader));
		result = validateDataItemPayload(Id, Data);
	}

	Trace("validateDataItem status: " + std::to_string(result) + ", " + err_msg[-result]);
	return result;
}

// factory method receives an entire DataItem in TBytes form.
// eg 10 02 05 00 00 00 00 80 3F is DAC_Scale1, DAC # 0, 3F80000
//    --DId --LEN --PayloadBytes
//       |     |  DAC      Scale
//     0210  0005   0    3F80000
PTDataItem TDataItem::fromBytes(TBytes bytes, TError &result)
{
	LOG_IT;
	result = ERR_SUCCESS;
	//Debug("Received = ", bytes);

	GUARD((bytes.size() >= sizeof(TDataItemHeader)), ERR_MSG_DATAITEM_TOO_SHORT, bytes.size());

	TDataItemHeader *head = (TDataItemHeader *)bytes.data();
	GUARD(isValidDataItemID(head->DId), ERR_DId_INVALID, static_cast<__u16>(head->DId));

	// PTDataItem anItem;
	TDataItemLength DataSize = head->dataLength; // MessageLength
	TBytes data;
	if (DataSize == 0)
	{
	}
	else
	{
		data.insert(data.end(), bytes.cbegin() + sizeof(TDataItemHeader), bytes.cbegin() + sizeof(TDataItemHeader) + DataSize);

		result = validateDataItemPayload(head->DId, data);
		if (result != ERR_SUCCESS)
		{
			Error("TDataItem::fromBytes() failed validateDataItemPayload with status: " + std::to_string(result) + ", " + err_msg[-result]);
			return PTDataItem(new TDataItem(DataItemIds::INVALID));
		}
	}
	//Debug("TDataItem::fromBytes sending to constructor: ", data);
	// auto item = DIdDict.find(head->DId)->second.Construct(head->DId, data); // TODO: FIX: WARN: FIX: WARN: ASfdafasdfasdfasdfasdfasdfas dfasd fasdf asd fasd fased f
	auto item = DIdDict.find(head->DId)->second.Construct(head->DId, data); // TODO: FIX: WARN: FIX: WARN: ASfdafasdfasdfasdfasdfasdfas dfasd fasdf asd fasd fased f
	Debug("Constructed Item as string:" + item->AsString());
	return item;
}

#pragma endregion

TDataItem::TDataItem(DataItemIds DId) : TDataItemParent(DId)
{
	Trace("ENTER, DId: " + to_hex<TDataId>(DId));
	this->setDId(DId);
}

TDataItem &TDataItem::addData(__u8 aByte)
{
	Data.push_back(aByte);
	return *this;
}

TDataItem &TDataItem::setDId(DataItemIds DId)
{
	GUARD(isValidDataItemID(DId), ERR_MSG_DATAITEM_ID_UNKNOWN, static_cast<__u16>(DId));
	this->Id = DId;
	return *this;
}

DataItemIds TDataItem::getDId()
{
	return this->Id;
}

bool TDataItem::isValidDataLength()
{
	bool result = false;
	DataItemIds DId = this->getDId();
	TDataItemLength len = this->Data.size();
	if ((this->getMinLength(DId) <= len) && (this->getMaxLength(DId) >= len))
	{
		result = true;
	}
	return result;
}

TBytes TDataItem::AsBytes(bool bAsReply)
{
	TBytes bytes;
	stuff<TDataId>(bytes, static_cast<__u16>(this->Id));
	this->Data = this->calcPayload(bAsReply);
	stuff<TDataItemLength>(bytes, this->Data.size());

	bytes.insert(end(bytes), begin(Data), end(Data));
	return bytes;
}

std::string TDataItem::getDIdDesc()
{
	return DIdDict.find(this->getDId())->second.desc;
}

// returns human-readable, formatted (multi-line) string version of this TDataItem
std::string TDataItem::AsString(bool bAsReply)
{
	std::stringstream dest;
	dest << this->getDIdDesc() << ", Data bytes: " << this->Data.size() << ": ";
	if (this->Data.size() != 0)
	{
		for (auto byt : this->Data)
		{
			dest << "0x" << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
		}
	}
	Trace("AsString produced: "+dest.str());
	return dest.str();
}

#pragma region Verbs-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- // TODO: fix; think this through
TDataItem &TDataItem::Go()
{
	LOG_IT;

	if (DIdDict.find(this->getDId())->second.go)
	{
		Debug("Lambda GO call...");
		DIdDict.find(this->getDId())->second.go((void *)Data.data());
	}
	else
	{
		Trace("ENTER - NYI!!!! : " + to_hex<__u16>(this->getDId()));
		this->resultCode = ERR_NYI;
	}
	return *this;
}

TError TDataItem::getResultCode()
{
	LOG_IT;

	Trace("resultCode: " + std::to_string(this->resultCode));
	return this->resultCode;
}

std::shared_ptr<void> TDataItem::getResultValue()
{
	LOG_IT;

	Trace("ENTER - TDataItem doesn't have a resultValue... returning 0");
	return std::shared_ptr<void>(0);
}

#pragma endregion
#pragma endregion TDataItem implementation

#pragma region TDataItemNYI implementation
// NYI (lol)
#pragma endregion
