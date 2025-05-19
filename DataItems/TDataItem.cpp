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
#include "DIO_.h"
#include "REG_.h"
#include "SYS_.h"
#include "../eNET-AIO16-16F.h"

#define DIdNYI(d) { DataItemIds::d, { 0, 0, 0, construct<TDataItemNYI>, #d " (NYI)" } }

#define DATA_ITEM_IMPL_2(x, aclass, a, b, c, y, z)                                                 \
	{                                                                                              \
		DataItemIds::x,                                                                                         \
		{                                                                                          \
			a, b, c, [](DataItemIds q, TBytes bytes) { return construct<aclass>(DataItemIds::x, bytes); }, y, z \
		}                                                                                          \
	}
#define DATA_ITEM_IMPL_1(x, aclass, a, b, c, y)                                                 \
	{                                                                                           \
		DataItemIds::x,                                                                                      \
		{                                                                                       \
			a, b, c, [](DataItemIds q, TBytes bytes) { return construct<aclass>(DataItemIds::x, bytes); }, y \
		}                                                                                       \
	}
#define DATA_ITEM_GET_MACRO(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define DATA_ITEM(...) DATA_ITEM_GET_MACRO(__VA_ARGS__, DATA_ITEM_IMPL_2, DATA_ITEM_IMPL_1)(__VA_ARGS__)

#pragma region TDataItemRaw

// stores raw bytes as parameters instead of struct-style
class TDataItemRaw : public TDataItem<GenericParams> {
public:
	TDataItemRaw(DataItemIds dId, const TBytes &bytes)
		: TDataItem<GenericParams>(dId, bytes)
	{}

	// No hardware action
	virtual TDataItemBase &Go() override {
		return *this;
	}

	virtual std::string AsString(bool bAsReply=false) override {
		return "TDataItemRaw, DId=" + to_hex<__u16>((__u16)this->DId)
				+ " " + getDIdDesc()
				+ ", size=" + std::to_string(this->rawBytes.size())
				+ ": " + to_hex(this->rawBytes);
	}
};

#pragma region DIdDict definition
const std::map<DataItemIds, TDIdDictEntry> DIdDict =
	{
		//{INVALID, { 9, 9, 9,( [](DataItemIds x, TBytes bytes) { return construct<TDataItem>(x, bytes); }), "DAC_Calibrate1(u8 iDAC, single Offset, single Scale)", nullptr}},
		DATA_ITEM(INVALID, TDataItemRaw, 0, 0, 0, "Invalid DId -1", nullptr),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region BRD_
#endif
		DATA_ITEM(BRD_,					TDataItemDoc,			0, 0, 0,   "Documentation: list of BRD_ DataItems", nullptr),
		DATA_ITEM(BRD_Reset,			TDataItemRaw,			0, 0, 0,   "BRD_Reset()", nullptr),
		DATA_ITEM(BRD_DeviceID,			TBRD_DeviceID,			0, 0, 0,   "BRD_DeviceID() → u32", nullptr),
		DATA_ITEM(BRD_Features,			TBRD_Features,			0, 4, 4,   "BRD_Features() → u8"),
		DATA_ITEM(BRD_FpgaID,			TBRD_FpgaId,			0, 4, 4,   "BRD_FpgaID() → u32"),
		DATA_ITEM(BRD_Model,            TBRD_Model,             12, 40, 40,"BRD_Model(ASCII)", nullptr),
		DATA_ITEM(BRD_GetModel,         TBRD_GetModel,          0, 0, 0,   "BRD_GetModel() → ASCII", nullptr),
		DATA_ITEM(BRD_SerialNumber,     TBRD_SerialNumber,      12, 12, 14,"BRD_SetSerialNumber(ASCII SN)", nullptr),
		DATA_ITEM(BRD_GetSerialNumber,  TBRD_GetSerialNumber,   0, 0, 0,   "BRD_GetSerialNumber() → ASCII", nullptr),
		DATA_ITEM(BRD_NumberOfSubmuxes,	TBRD_NumberOfSubmuxes,	1, 1, 1,   "BRD_NumberOfSubmuxes(u8 count)", nullptr),
		DATA_ITEM(BRD_SubmuxScale,		TBRD_SubmuxScale,		6, 6, 6,   "BRD_SubmuxScale(u8 submuxIndex, u8 gainGroupIndex, f32 Scale)", nullptr),
		DATA_ITEM(BRD_SubmuxOffset,		TBRD_SubmuxOffset,		6, 6, 6,   "BRD_SubmuxOffset(u8 submuxIndex, u8 gainGroupIndex, f32 Offset)", nullptr),
		DATA_ITEM(BRD_GetNumberOfSubmuxes, TBRD_GetNumberOfSubmuxes, 0, 0, 0, "BRD_GetNumberOfSubmuxes() → [u8]", nullptr),
		DATA_ITEM(BRD_GetSubmuxScale,   TBRD_GetSubmuxScale,    2, 2, 2,   "BRD_GetSubmuxScale(u8 submuxIndex, u8 gainGroupIndex) → [float]", nullptr),
		DATA_ITEM(BRD_GetSubmuxOffset,  TBRD_GetSubmuxOffset,   2, 2, 2,   "BRD_GetSubmuxOffset(u8 submuxIndex, u8 gainGroupIndex) → [float]", nullptr),

		DATA_ITEM(BRD_REBOOT,   TDataItemRaw,  8, 8, 8,   "BRD_REBOOT(double)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						double token = *(double *)args;
						if (token == M_PI)
							done = true;
					})),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region REG_
#endif
		// the registers on the eNET are only accessible as 8- or 32-bits, depending on the specific register.
		// the "in()" and "out()" functions deal with this
		DATA_ITEM(REG_,       TDataItemDoc,  0, 0, 0,   "Documentation: list of REG_ DataItems", nullptr),

		DATA_ITEM(REG_Read1,  TREG_Read1,  1, 1, 1, "REG_Read1(u8 offset) → [u8|u32]",   nullptr),
		DIdNYI(REG_ReadBuf),
		DATA_ITEM(REG_Write1, TREG_Write1, 2, 5, 5, "REG_Write1(u8 ofs, [u8|u32] data)", nullptr),
		DIdNYI(REG_WriteBuf),

		DATA_ITEM(REG_ClearBits, TDataItemRaw, 2, 5, 5,
				  "REG_ClearBits(u8 ofs, u8|u32 bitsToClear)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 * pargs = (__u8 *)args;
						__u8 ofs = *pargs++;
						__u32 bitsToClear = regextract(pargs, ofs);

						__u32 regValue = in(ofs);
						regValue &= ~ bitsToClear;
						out(ofs, regValue);
					})),
		DATA_ITEM(REG_ToggleBits, TDataItemRaw, 2, 5, 5,
				  "REG_ToggleBits(u8 ofs, u8|u32 bitsToToggle)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 * pargs = (__u8 *)args;
						__u8 ofs = *pargs++;
						__u32 bitsToToggle = regextract(pargs, ofs);

						__u32 regValue = in(ofs);
						regValue ^= bitsToToggle;
						out(ofs,regValue);
					})),
		DATA_ITEM(REG_SetBits, TDataItemRaw, 2, 5, 5, "REG_SetBits(u8 ofs, u8|u32 bitsToSet)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 * pargs = (__u8 *)args;
						__u8 ofs = *pargs++;
						__u32 bitsToSet = regextract(pargs, ofs);

						__u32 regValue = in(ofs);
						regValue |= bitsToSet;
						out(ofs,regValue);
					})),
		DATA_ITEM(REG_ReadBit,   TREG_ReadBit,               2, 2, 2, "TREG_ReadBit(u8 offset, u8 bitIndex) → [u8]",           nullptr),
		DATA_ITEM(REG_WriteBit,  TREG_WriteBit,              3, 3, 3, "TREG_WriteBit(u8 offset, u8 bitIndex, u8 one_or_zero)", nullptr),
		DATA_ITEM(REG_ClearBit,  TREG_ClearBit,              2, 2, 2, "TREG_ClearBit(u8 offset, u8 bitIndex)",                 nullptr),
		DATA_ITEM(REG_SetBit,    TREG_SetBit,                2, 2, 2, "TREG_SetBit(u8 offset, u8 bitIndex)",                   nullptr),
		DATA_ITEM(REG_ToggleBit, TREG_ToggleBit,             2, 2, 2, "TREG_ToggleBit(u8 offset, u8 bitIndex)",                nullptr),

//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region DAC_
#endif
		// There are four DAC outputs on this board, driven by a pair of dual-DAC chips on an SPI bus (distinct from the DIO SPI)
		// The range of each DAC is factory-set, per-dac, with ±10, ±5, 0-10, and 0-5 "standard"
		// aioenetd is configured at the factory for the DAC range, so customers can output in Voltage
		DATA_ITEM(DAC_,          TDataItemDoc,               0, 0, 0, "Documentation: list of DAC_ DataItems", nullptr),
		DATA_ITEM(DAC_Output1,   TDAC_Output,                5, 5, 5, "DAC_Output1(u8 iDAC, single Volts)", nullptr),
		DATA_ITEM(DAC_Range1,    TDAC_Range1,                5, 5, 5, "DAC_Range1(u8 iDAC, u32 RangeCode)", nullptr),
		DIdNYI(DAC_Configure1),
		DIdNYI(DAC_ConfigAndOutput1),
		DATA_ITEM(DAC_Calibrate1, TDataItemRaw,              9, 9, 9, "DAC_Calibrate1(u8 iDAC, single Offset, single Scale)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 dacnum = *(((__u8 *)args) + 0);
						GUARD(dacnum < 4, ERR_DId_BAD_PARAM, dacnum);
						float offset = *(float *)(((__u8 *)args) + 1);
						float scale = *(float *)(((__u8 *)args) + 5);
						Config.dacScaleCoefficients[dacnum] = scale;
						Config.dacOffsetCoefficients[dacnum] = offset;
						Debug("inside lambda: DAC " + std::to_string(dacnum) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum])
						    + ", offset = " + std::to_string(Config.dacOffsetCoefficients[dacnum]));
					})),

		DATA_ITEM(DAC_Offset1, TDataItemRaw,                 5, 5, 5, "DAC_Offset1(u8 iDac, single Offset)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 dacnum = *(((__u8 *)args) + 0);
						GUARD(dacnum < 4, ERR_DId_BAD_PARAM, dacnum);
						float offset = *(float *)(((__u8 *)args) + 1);
						Config.dacOffsetCoefficients[dacnum] = offset;
						Debug("inside lambda: DAC " + std::to_string(dacnum) + ", offset = " + std::to_string(Config.dacOffsetCoefficients[dacnum]));
					})),

		DATA_ITEM(DAC_OffsetAll, TDataItemRaw,               16, 16, 16, "DAC_OffsetAll(single offset0, offset1, offset2, offset3)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						for (__u8 dacnum = 0; dacnum < NUM_DACS; ++dacnum)
						{
							float offset = *(float *)(((__u8 *)args) + dacnum*4);
							Config.dacOffsetCoefficients[dacnum] = offset;
							Debug("inside lambda: DAC " + std::to_string(dacnum) + ", offset = " + std::to_string(Config.dacOffsetCoefficients[dacnum]));
						}
					})),
		DATA_ITEM(DAC_Scale1, TDataItemRaw,                  5, 5, 5, "DAC_Scale1(u8 iDac, single Scale)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						__u8 dacnum = *(((__u8 *)args) + 0);
						GUARD(dacnum < 4, ERR_DId_BAD_PARAM, dacnum);
						float scale = *(float *)(((__u8 *)args) + 1);
						Config.dacScaleCoefficients[dacnum] = scale;
						Debug("inside lambda: DAC " + std::to_string(dacnum) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]));
					})),

		DATA_ITEM(DAC_ScaleAll, TDataItemRaw,                16, 16, 16, "DAC_ScaleAll(single scale0, scale1, scale2, scale3)",
				  static_cast<std::function<void(void *)>>([](void *args)
					{
						for (__u8 dacnum = 0; dacnum < NUM_DACS; ++dacnum)
						{
							float scale = *(float *)(((__u8 *)args) + dacnum*4);
							Config.dacScaleCoefficients[dacnum] = scale;
							Debug("inside lambda: DAC " + std::to_string(dacnum) + ", scale = " + std::to_string(Config.dacScaleCoefficients[dacnum]));
						}
					})),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region DIO_
#endif
		DATA_ITEM(DIO_,             TDataItemDoc,               0, 0, 0,   "Documentation: list of DIO_ DataItems", nullptr),

	// there are 16 digital bits.  All of them are individually configured as input vs output.  1 = input, 0 = output // CHECK // FIX // TODO:
	// this function sets the direction of all 16 bits, and the initial output level (high/low) of any bits configured for output.  Data is passed
	// as a 16-bit word
	// DATA @ ofsDioOutputs for writes and @ ofsDioInputs for reads
	// bytes:       1                 0
	// bits: F E D C B A 9 8   7 6 5 4 3 2 1 0
	// Notes:@ * * *           + + + + + + + +

	// DIRECTION CONTROL @ ofsDioDirections
	// bytes:       1                 0
	// bits: F E D C B A 9 8   7 6 5 4 3 2 1 0
	// Notes:@ * * *           + + + + + + + +

	// @ this bit is available for use as PWM output or input
	// * 3 bits are consumed when a submux is attached. They are forced to output and are under FPGA control; writes are ignored
	// + these 8 bits are SPI-driven thus slower
		DATA_ITEM(DIO_Configure,    TDIO_Configure,          2, 2, 2, "TDIO_Configure(u16 value) - each bit: 1=input, 0=output", nullptr),
		DATA_ITEM(DIO_Input,        TDIO_Input,              0, 0, 0, "TDIO_Input() → returns u16 value", nullptr),
		DATA_ITEM(DIO_Output,       TDIO_Output,             2, 2, 2, "TDIO_Output(u16 value) - sets digital outputs", nullptr),
		DATA_ITEM(DIO_ConfigureBit, TDIO_ConfigureBit,       2, 2, 2, "DIO_ConfigureBit(u8 bitNumber, u8 direction)", nullptr),
		DATA_ITEM(DIO_InputBit,     TDIO_InputBit,           1, 1, 1, "DIO_InputBit(u8 bitNumber) → [u8]", nullptr),
		DATA_ITEM(DIO_OutputBit,    TDIO_OutputBit,          2, 2, 2, "DIO_OutputBit(u8 bitNumber, u8 value)", nullptr),
		DATA_ITEM(DIO_ClearBit,     TDIO_ClearBit,           1, 1, 1, "DIO_ClearBit(u8 bitNumber)", nullptr),
		DATA_ITEM(DIO_SetBit,       TDIO_SetBit,             1, 1, 1, "DIO_SetBit(u8 bitNumber)", nullptr),
		DATA_ITEM(DIO_ToggleBit,    TDIO_ToggleBit,          1, 1, 1, "DIO_ToggleBit(u8 bitNumber)", nullptr),

		DIdNYI(DIO_PulseBit),
		DIdNYI(DIO_ConfigureReadWriteReadSome),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region ADC_
#endif
		DATA_ITEM(ADC_,               TDataItemDoc,          0, 0, 0,   "Documentation: list of ADC_ DataItems", nullptr),

		DIdNYI(ADC_Claim),
		DIdNYI(ADC_Release),
		DATA_ITEM(ADC_BaseClock,      TADC_BaseClock,        0, 0, 4, "ADC_BaseClock() → u32", nullptr),
		DATA_ITEM(ADC_StartHz,        TDataItemRaw,          4, 4, 4, "ADC_StartHz(f32)", nullptr),
		DATA_ITEM(ADC_StartDivisor,   TDataItemRaw,          4, 4, 4, "ADC_StartDivisor(u32)", nullptr),
		DIdNYI(ADC_ConfigurationOfEverything),

		DATA_ITEM(ADC_Differential1,   TADC_Differential1,   2, 2, 2, "ADC_Differential1(u8 channelGroup, u8 singleEnded)", nullptr),
		DATA_ITEM(ADC_DifferentialAll, TADC_DifferentialAll, 8, 8, 8, "ADC_DifferentialAll(8 bytes: one per channelGroup)", nullptr),

		DATA_ITEM(ADC_Range1,          TADC_Range1,          2, 2, 2, "ADC_Range1(u8 channelGroup, u8 range)", nullptr),
		DATA_ITEM(ADC_RangeAll,        TADC_RangeAll,        8, 8, 8, "ADC_RangeAll(8 bytes: range for each channelGroup)", nullptr),

		DATA_ITEM(ADC_Scale1,          TADC_Scale1,          5, 5, 5, "ADC_Scale1(u8 rangeIndex, float scale)", nullptr),
		DATA_ITEM(ADC_ScaleAll,        TADC_ScaleAll,        8*sizeof(float), 8*sizeof(float), 8*sizeof(float), "ADC_ScaleAll(8 floats for scale calibration)", nullptr),

		DATA_ITEM(ADC_Offset1,         TADC_Offset1,         5, 5, 5, "ADC_Offset1(u8 rangeIndex, float offset)", nullptr),
		DATA_ITEM(ADC_OffsetAll,       TADC_OffsetAll,       8*sizeof(float), 8*sizeof(float), 8*sizeof(float), "ADC_OffsetAll(8 floats for offset calibration)", nullptr),

		DATA_ITEM(ADC_Calibration1,    TADC_Calibration1,    9, 9, 9, "ADC_Calibration1(u8 rangeIndex, float scale, float offset)", nullptr),
		DATA_ITEM(ADC_CalibrationAll,  TADC_CalibrationAll,  16*sizeof(float), 16*sizeof(float), 16*sizeof(float), "ADC_CalibrationAll(8 scales followed by 8 offsets)", nullptr),

		DIdNYI(ADC_Volts1),
		DIdNYI(ADC_VoltsAll), // ADC_GetScanV
		DIdNYI(ADC_Counts1),
		DIdNYI(ADC_CountsAll), // ADC_GetScanCounts
		DIdNYI(ADC_Raw1),
		DIdNYI(ADC_RawAll),   // ADC_GetScanRaw
		DATA_ITEM(ADC_StreamStart,     TDataItemRaw,         4, 4, 4, "ADC_StreamStart((u32)AdcConnectionId)", nullptr),
		DATA_ITEM(ADC_StreamStop,      TDataItemRaw,         0, 0, 0, "ADC_StreamStop()", nullptr),

		// DIdNYI(ADC_Streaming_stuff_including_Hz_config),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region SCRIPT_
#endif
		DIdNYI(SCRIPT_Pause), // SCRIPT_Pause(__u8 delay ms)
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region WDG_
#endif
		DATA_ITEM(WDG_,          TDataItemDoc,               0, 0, 0,   "Documentation: list of WDG_ DataItems", nullptr),

//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region DEF_
#endif
		DATA_ITEM(DEF_,          TDataItemDoc,               0, 0, 0,   "Documentation: list of DEF_ DataItems", nullptr),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region SERVICE_
#endif
		DATA_ITEM(SERVICE_,      TDataItemDoc,               0, 0, 0,   "Documentation: list of SERVICE_ DataItems", nullptr),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region PNP_
#endif
		DATA_ITEM(PNP_,          TDataItemDoc,               0, 0, 0,   "Documentation: list of PNP_ DataItems", nullptr),

//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region CFG_
#endif
		DATA_ITEM(CFG_,          TDataItemDoc,               0, 0, 0,   "Documentation: list of CFG_ DataItems", nullptr),
		DATA_ITEM(CFG_Hostname,  TDataItemRaw,               1, 20, 253, "CFG_Hostname({valid Hostname})", nullptr),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region SYS_
#endif
		DATA_ITEM(SYS_UploadFileName, TSYS_UploadFileName,   1, 255, 255, "SYS_UploadFileName({valid filepath})", nullptr),
		DATA_ITEM(SYS_UploadFileData, TSYS_UploadFileData,   1, 65534, 65534, "SYS_UploadFileData({valid file data})", nullptr),
//---------------------------------------------------------------------------------------------------------------------------------
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region PWM_
#endif
		DATA_ITEM(PWM_,          TDataItemDoc,               0, 0, 0,   "Documentation: list of PWM_ DataItems", nullptr),

		DIdNYI(PWM_Configure1),
		DIdNYI(PWM_Input1),
		DIdNYI(PWM_Output1),
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region TCP_
#endif
		DATA_ITEM(TCP_,             TDataItemDoc,            0, 0, 0, "Documentation: list of TCP_ DataItems", nullptr),
		DATA_ITEM(TCP_ConnectionID, TDataItemRaw,            4, 4, 4, "TCP_ConnectionID() → u32", nullptr),
#if defined(_MSC_VER) || defined(__clang__)
	#pragma region DOC_
#endif
		DATA_ITEM(DOC_Get,          TDataItemDocGet,         0, 0, 0, "Documentation: list of All top-level DataItem groups", nullptr),
};

#pragma endregion



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
#pragma region TDataItemBase Class Methods(static)
// NYI - validate the payload of a Data Item based on the Data Item ID
// e.g, if the Message is equivalent to ADC_SetRangeAll(__u8 ranges[16]) then the Data
//	  should be 16 bytes, each of which is a valid Range Code
// e.g, if the Message is ADC_SetRange(__u16 Channel, __u8 RangeCode) then the Data
//	  should be 3 bytes, being a __u16 Channel which must be valid for this device, and
//	  a valid RangeCode byte
// NOTE:
//   This should be implemented OOP-style: each TDataItem ID should be a descendant-class that provides the
//   specific validate and parse appropriate to that DataItemID
int TDataItemBase::validateDataItemPayload(DataItemIds DId, const TBytes &bytes)
{
	//Trace("ENTER, DId: " + to_hex<TDataId>(DId) + ": ", bytes);
	int result = ERR_MSG_PAYLOAD_DATAITEM_LEN_MISMATCH;
	TDataItemLength len = static_cast<TDataItemLength>(bytes.size());
	Trace(std::to_string(getMinLength(DId)) + " <= " + std::to_string(len) + " <= " + std::to_string(getMaxLength(DId)));
	if ((TDataItemBase::getMinLength(DId) <= len) && (len <= TDataItemBase::getMaxLength(DId)))
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

std::string TDataItemBase::AsStringBase(bool bAsReply) const {
    auto it = DIdDict.find(DId);
    std::string desc = (it != DIdDict.end()) ? it->second.desc : "Unknown DId";
    return desc + " (DId=" + std::to_string(static_cast<__u16>(DId)) + ")";
}

int TDataItemBase::getDIdIndex(DataItemIds DId)
{
	auto item = DIdDict.find(DId);
	if (item != DIdDict.end())
	{
		return static_cast<int>(std::distance(DIdDict.begin(), item));
	}
	throw std::logic_error("DId not found in list");
}

// returns human-readable description of this TDataItem
std::string TDataItemBase::getDIdDesc(DataItemIds DId)
{
	return DIdDict.find(DId)->second.desc;
}

TDataItemLength TDataItemBase::getMinLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.minLen;
}

TDataItemLength TDataItemBase::getTargetLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.maxLen;
}

TDataItemLength TDataItemBase::getMaxLength(DataItemIds DId)
{
	return DIdDict.find(DId)->second.maxLen;
}

int TDataItemBase::isValidDataItemID(DataItemIds DId)
{
	Debug("DataItemId: " + to_hex<__u16>((static_cast<__u16>(DId))));
	auto item = DIdDict.find(DId);
	return (item != DIdDict.end());
}

int TDataItemBase::validateDataItem(const TBytes &bytes)
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
		Error(err_msg[-ERR_MSG_DATAITEM_ID_UNKNOWN]);
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
std::shared_ptr<TDataItemBase> TDataItemBase::fromBytes(const TBytes &bytes, TError &result)
{
	LOG_IT;
	result = ERR_SUCCESS;
	//Debug("Received = ", bytes);

	GUARD((bytes.size() >= sizeof(TDataItemHeader)), ERR_MSG_DATAITEM_TOO_SHORT, static_cast<int>(bytes.size()));

	TDataItemHeader *head = (TDataItemHeader *)bytes.data();
	GUARD(isValidDataItemID(head->DId), ERR_DId_INVALID, static_cast<__u16>(head->DId));

	// PTDataItem anItem;
	TDataItemLength DataSize = head->dataLength; // MessageLength
	TBytes data{};
	if (DataSize == 0)
	{
	}
	else
	{
		data.insert(data.end(), bytes.cbegin() + sizeof(TDataItemHeader), bytes.cbegin() + sizeof(TDataItemHeader) + DataSize);

		result = validateDataItemPayload(head->DId, data);
		if (result != ERR_SUCCESS)
		{
			Error("TDataItem::fromBytes() failed validateDataItemPayload with status: "
				  + std::to_string(result) + ", " + err_msg[-result]);
			return std::make_shared<TDataItemNYI>(DataItemIds::INVALID, TBytes{});
		}
	}
	//Debug("TDataItem::fromBytes sending to constructor: ", data);
	// auto item = DIdDict.find(head->DId)->second.Construct(head->DId, data); // TODO: FIX: WARN: FIX: WARN: ASfdafasdfasdfasdfasdfasdfas dfasd fasdf asd fasd fased f
	auto item = DIdDict.find(head->DId)->second.Construct(head->DId, data); // TODO: FIX: WARN: FIX: WARN: ASfdafasdfasdfasdfasdfasdfas dfasd fasdf asd fasd fased f
	Log("Constructed Item as string:" + item->AsString(), item->Data);
	return item;
}

#pragma endregion

TDataItemBase &TDataItemBase::addData(__u8 aByte)
{
	this->Data.push_back(aByte);
	return *this;
}

TDataItemBase &TDataItemBase::setDId(DataItemIds DId)
{
	GUARD(isValidDataItemID(DId), ERR_MSG_DATAITEM_ID_UNKNOWN, static_cast<__u16>(DId));
	this->DId = DId;
	return *this;
}

DataItemIds TDataItemBase::getDId() const
{
	return this->DId;
}

bool TDataItemBase::isValidDataLength() const
{
	bool result = false;
	DataItemIds DId = this->getDId();
	TDataItemLength len = static_cast<TDataItemLength>(this->Data.size());
	if ((this->getMinLength(DId) <= len) && (this->getMaxLength(DId) >= len))
	{
		result = true;
	}
	return result;
}

TBytes TDataItemBase::AsBytes(bool bAsReply)
{
	TBytes bytes;
	stuff<TDataId>(bytes, static_cast<__u16>(this->DId));
	this->Data = this->calcPayload(bAsReply);
	stuff<TDataItemLength>(bytes, static_cast<TDataItemLength>(this->Data.size()));

	bytes.insert(end(bytes), begin(Data), end(Data));
	return bytes;
}

std::string TDataItemBase::getDIdDesc() const
{
	return DIdDict.find(this->getDId())->second.desc;
}

// returns human-readable, formatted (multi-line) string version of this TDataItem
std::string TDataItemBase::AsString(bool bAsReply)
{
	std::stringstream dest,deststr;
	dest << this->getDIdDesc() << ", Data bytes: " << this->Data.size() << ": ";
	if (this->Data.size() != 0)
	{
		for (auto byt : this->Data)
		{
			dest << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " ";
			deststr << static_cast<char>(byt);
		}
	}
	Trace("AsString produced: "+dest.str()+" ["+deststr.str()+"]");
	return dest.str();
}

#pragma region Verbs-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- // TODO: fix; think this through
TDataItemBase &TDataItemBase::Go()
{
	LOG_IT;

	if (DIdDict.find(this->getDId())->second.go)
	{
		Debug("Lambda GO call...");
		DIdDict.find(this->getDId())->second.go((void *)Data.data());
	}
	else
	{
//		Trace("ENTER - NYI!!!! : " + to_hex<__u16>(this->getDId()));
		this->resultCode = ERR_NYI;
	}
	return *this;
}

TError TDataItemBase::getResultCode()
{
	LOG_IT;

	Trace("resultCode: " + std::to_string(this->resultCode));
	return this->resultCode;
}

std::shared_ptr<void> TDataItemBase::getResultValue()
{
	LOG_IT;

	Trace("ENTER - TDataItem doesn't have a resultValue... returning 0");
	return std::shared_ptr<void>(0);
}

#pragma endregion
#pragma endregion TDataItem implementation

#pragma region TDataItemDoc
static inline __u8 GetMSB(DataItemIds id) {
    return static_cast<__u8>(static_cast<unsigned>(id) >> 8);
}

TDataItemDoc::TDataItemDoc(DataItemIds DId, const TBytes &data)
    : TDataItem<DOC_Params>(DId, data)
{
    // Ignore incoming payload.
}

TDataItemDoc::TDataItemDoc(DataItemIds DId)
    : TDataItem<DOC_Params>(DId, {})
{
}

TDataItemBase &TDataItemDoc::Go() {
    // The group is the MSB of our own DataItemId.
    __u8 group = GetMSB(this->DId);
    std::stringstream ss;
    // Iterate over all entries in the global DIdDict.
    for (const auto &entry : DIdDict) {
        DataItemIds id = entry.first;
        __u8 msb = GetMSB(id);
        __u8 lsb = static_cast<__u8>(static_cast<unsigned>(id) & 0xFF);
        // Only include items from the same group (MSB equals) and with LSB != 0.
        if (msb == group && lsb != 0) {
            ss << entry.second.desc << "\n";
        }
    }
    std::string payload = ss.str();
    // Store the assembled string into our Data vector.
    this->Data.assign(payload.begin(), payload.end());
    return *this;
}

TBytes TDataItemDoc::calcPayload(bool /*bAsReply*/) {
    return this->Data;
}

std::string TDataItemDoc::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDataItemDoc(group=0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(GetMSB(this->DId)) << ")";
    return ss.str();
}

#pragma endregion

#pragma region TDataItemDocGet
TDataItemDocGet::TDataItemDocGet(DataItemIds DId, const TBytes &data)
    : TDataItem<DOC_Get_Params>(DId, data)
{
    // No parameters expected.
}

TDataItemDocGet::TDataItemDocGet(DataItemIds DId)
    : TDataItem<DOC_Get_Params>(DId, {})
{
}

TDataItemBase &TDataItemDocGet::Go() {
    std::stringstream ss;
    // Iterate through the global dictionary.
    for (const auto &entry : DIdDict) {
        // The low byte of the DataItemId:
        __u8 lsb = static_cast<__u8>(static_cast<unsigned>(entry.first) & 0xFF);
        // Only include items where LSB is 0.
        if (lsb == 0) {
            ss << entry.second.desc << "\n";
        }
    }
    std::string docPayload = ss.str();
    // Store the assembled documentation into the Data field.
    this->Data.assign(docPayload.begin(), docPayload.end());
    return *this;
}

TBytes TDataItemDocGet::calcPayload(bool /*bAsReply*/) {
    return this->Data;
}

std::string TDataItemDocGet::AsString(bool /*bAsReply*/) {
    std::stringstream ss;
    ss << "TDataItemDocGet(DId=0x" << std::hex << std::setw(4) << std::setfill('0')
       << static_cast<unsigned>(this->DId) << ")";
    return ss.str();
}
#pragma endregion

#pragma region TDataItemNYI implementation
// NYI (lol)
#pragma endregion
