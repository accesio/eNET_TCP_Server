/*
TDataItemParent - virtual / interface
	Provides a base class that defines the basic functionality and data associated with every action or query available
	to the eNET- protocol as encapsulated by serialized Data Items.

	Every serialized DataItem has a type code, a payload length, and an optional payload (called "Data").  The
	TDataItemParent deserializes this packet in its constructor, creating an Object of type TDataItem(descendant).

	The private fields in TDataItemParent are virtually identical to the serialized form of the Data Item: a DataItemId
	identical to the type code, and the Data as a TBytes (vector<__u8>) which maintains the payload length and content.

	TDataItemParent should have a factory function that takes a serialized DataItem and parses it for validity then
	constructs the appropriate descendant type as found in the dictionary<DataItemID, struct DataItemInfoStructThingy>

	However, the behavior of all DataItems is structually identical ...
	)	TDataItems descendants have a constructor
	)	TDataItems have a "Go()" function that the ActionThread executes as part of the TMessage execution.
	)	TDataItems have a "calcPayload(bAsReply)" function that the send thread executes to serialize the results into a
		packet for sending to the client.
	... thus it is plausible to define these three functions instead of a descendant class, and pass the function pointers as
	part of the DataItemInfoStructThingy.  Doing so is the non-class/OOP technique.


	The goal of this discussion is to refactor the existing Data Item class hierarchy into a clean, steamlined, and logical
	concept.

	basically, TDataItemParent should act like TObject in the Delphi VCL and each descendant should add minimally.

	First, a bunch of static functions should move from TDataItem to the TDataItemParent.  Others should be deleted.  (A bunch
	of code is conceptually needed for consumers of TMessage et al who act as *clients*, not a concern at the moment, worry
	about it later!)
*/

#pragma once
#include <functional>
#include <map>
#include <iterator>
#include <fmt/core.h>

#include "../logging.h"
#include "../utilities.h"
#include "../eNET-AIO16-16F.h"
#include "../TError.h"
#include "../apci.h"

class TDataItem;

typedef std::shared_ptr<TDataItem> PTDataItem;
typedef std::vector<PTDataItem> TPayload;
typedef __u16 TDataId;
typedef __u16 TDataItemLength;

#pragma region TDataItem DId enum

#define _INVALID_DATAITEMID_ ((TDataId)-1)

enum DataItemIds : TDataId
{
	INVALID = _INVALID_DATAITEMID_,
	// Note 1: the "TLA_" DIds (e.g., `BRD_` and `REG_` et al; i.e., those DId names that don't have anything after the `_`)
	//         return human-readable text of all TLA_ category DIds and what they do & why
	BRD_ = 0x0000, // Query Only.
	BRD_Reset = 0x0001,
	BRD_DeviceID,
	BRD_Features,
	BRD_FpgaID,
	BRD_stuff_needed_for_control_and_diagnostics_of_Linux_TCPIP_WDG_DEF_ETC, // TBD, long list
	BRD_REBOOT = 0xFF,

	REG_ = 0x100, // Query Only.
	// NOTE: REG_ister access functionality does not allow (at this time) specifying 8-. 16-, or 32-bit access width.
	//       Instead, the width is determined automatically from the register offset, because none of the eNET-AIO registers are flexible
	//    Additionally, the TDataItem::fromBytes() factory method will throw an error if an invalid offset is passed,
	//       like offset=0x41 is invalid because 0x40 is a 32-bit register
	// NOTE: int widthFromOffset(int ofs) is used to determine the register width but it is hard-coded, specific to eNET-AIO, by ranges
	//       of offsets. We'll want the aioenetd to eventually support OTHER (non-eNET-AIO16-128A Family) eNET- boards so this will
	//       need to be a configurable, preferably read off the hardware (although "from eMMC" is probably sufficient)
	REG_Read1 = 0x101,
	REG_ReadAll = 0x103,
	REG_ReadBuf, // like draining a FIFO, TODO: improve name
	REG_Write1 = 0x105,
	REG_WriteBuf = 0x107, // like filling a FIFO, TODO: improve name

	REG_ClearBits = 0x0108,
	REG_SetBits = 0x0109,
	REG_ToggleBits = 0x010A,

	DAC_ = 0x200, // Query Only. *1
	DAC_Output1 = 0x201,
	DAC_OutputAll,
	DAC_Range1 = 0x204, // Query Only.
	DAC_Calibrate1 = 0x20C,
	DAC_CalibrateAll,
	DAC_Offset1 = 0x20E,
	DAC_OffsetAll = 0x20F,
	DAC_Scale1 = 0x210,
	DAC_ScaleAll = 0x211,

	DIO_ = 0x300, // Query Only. *1
	DIO_Configure1,
	DIO_ConfigureAll,
	DIO_Input1,
	DIO_InputAll = 0x0305,
	DIO_InputBuf1,
	DIO_InputBufAll,
	DIO_Output1,
	DIO_OutputAll,
	DIO_OutputBuf, // like unpaced waveform output; NOTE: not sure this is useful
	DIO_ConfigureReadWriteReadSome,
	DIO_Clear1,
	DIO_ClearAll,
	DIO_Set1,
	DIO_SetAll,
	DIO_SetSome,
	DIO_Toggle1,
	DIO_ToggleAll,
	DIO_Pulse1,
	DIO_PulseAll,

	PWM_ = 0x400, // Query Only. *1
	PWM_Configure1,
	PWM_ConfigureAll,
	PWM_Input1,
	PWM_InputAll,
	PWM_Output1,
	PWM_OutputAll,

	ADC_ = 0x1000, // Query Only. *1
	ADC_Claim = 0x1001,
	ADC_Release = 0x1002,
	ADC_BaseClock = 0x1003,
	ADC_StartHz = 0x1004,
	ADC_StartDivisor = 0x1005,
	ADC_ConfigurationOfEverything = 0x1006,
	ADC_Differential1 = 0x1007,
	ADC_DifferentialAll = 0x1008,
	ADC_Range1 = 0x1009,
	ADC_RangeAll = 0x100A,
	ADC_Calibration1 = 0x100C,
	ADC_CalibrationAll = 0x100D,
	ADC_Scale1 = 0x100E,
	ADC_ScaleAll = 0x100F,
	ADC_Offset1 = 0x1010,
	ADC_OffsetAll = 0x1011,
	ADC_Volts1 = 0x1020,
	ADC_VoltsAll = 0x1021,
	ADC_Counts1 = 0x1030,
	ADC_CountsAll = 0x1031,
	ADC_Raw1 = 0x1040,
	ADC_RawAll = 0x1041,

	ADC_Stream = 0x1100,
	ADC_StreamStart = 0x1101,
	ADC_StreamStop = 0x1102,

	ADC_Streaming_stuff_including_Hz_config, // TODO: finish

	// TODO: DIds below this point are TBD/notional
	SCRIPT_ = 0x3000,
	SCRIPT_Pause, // insert a pause in execution of TDataItems

	// broken out from "BRD_stuff_needed_for_control_and_diagnostics_of_Linux_TCPIP_WDG_DEF_ETC" mentioned above
	WDG_ = 0x4000, // Watchdog related
	DEF_ = 0x5000, // power-on default state related
	SERVICE_,	   // tech support stuff
	TCP_ = 0x7000, // TCP-IP stuff broken out from the
	TCP_ConnectionID = 0x7001,
	PNP_,		   // distinct from BRD_?
	CFG_ = 0x9000, // "Other" Configuration stuff; Linux, IIoT protocol selection, etc?
	CFG_Hostname,
};
#pragma endregion

#pragma pack(push, 1)

int validateDataItemPayload(DataItemIds DataItemID, TBytes Data);
// return register width for given offset as defined for eNET-AIO registers
// returns 0 if offset is invalid
int widthFromOffset(int ofs);

#pragma region inserting ancestor under TDataItem
class TDataItemParent
{
public:
	TDataItemParent(DataItemIds DId) : Id(DId){};
	TDataItemParent(DataItemIds DId, TBytes data) : Id(DId), Data(data)
	{
		Debug("DataItemParent()");
	};
	TDataItemParent &setData(TBytes bytes)
	{
		Data = bytes;
		return *this;
	};
	DataItemIds Id{DataItemIds(0)};
	TBytes Data;
};

#pragma endregion

typedef std::unique_ptr<TDataItem> DIdConstructor(DataItemIds DId, TBytes FromBytes);

template <class q>
std::unique_ptr<TDataItem> construct(DataItemIds DId, TBytes FromBytes)
{
	return std::unique_ptr<TDataItem>(new q(DId, FromBytes));
}

typedef struct
{
	DataItemIds DId;
	TDataItemLength dataLength;
} TDataItemHeader;

typedef struct __DIdDictEntry_inner
{
	TDataItemLength minLen;
	TDataItemLength expectedLen;
	TDataItemLength maxLen;
	// std::function<std::unique_ptr<TDataItem>(TBytes)> Construct;
	DIdConstructor *Construct;
	std::string desc;
	std::function<void(void *)> go;
} TDIdDictEntry;

//  __DIdDictEntry_inner ;
extern const std::map<TDataId, TDIdDictEntry> DIdDict;

typedef struct __DIdDictEntry_inner2
{
	std::function<void(void *)> init;
	std::function<void(void *)> go;
	std::function<void(void *)> calcPayload;
	std::function<void(void *)> print;
	TDataItemLength minLen;
	TDataItemLength typLen;
	TDataItemLength maxLen;
	std::string desc;
} TDIdDictEntry2;

const std::map<TDataId, TDIdDictEntry2> DIdDict2 =
	{
		//{INVALID, { 9, 9, 9,( [](DataItemIds x, TBytes bytes) { return construct<TDataItem>(x, bytes); }), "DAC_Calibrate1(u8 iDAC, single Offset, single Scale)", nullptr}},
		{INVALID, {init : nullptr, go : nullptr, calcPayload : nullptr, print : nullptr, minLen : 0, typLen : 0, maxLen : 0, desc : "Invalid DId -1"}},
		{BRD_, {init : nullptr, go : nullptr, calcPayload : nullptr, print : nullptr, minLen : 0, typLen : 0, maxLen : 0, desc : "Invalid DID 0"}},
		{BRD_Reset, {init : nullptr,
					 go : [](void *args) -> void
					 {
						 out(ofsReset, bmResetEverything);
					 },
					 calcPayload : nullptr,
					 print : nullptr,
					 minLen : 0,
					 typLen : 0,
					 maxLen : 0,
					 desc : "BRD_Reset()"}},
};






#pragma region "class TDataItem" declaration
class DataItem
{
protected:
	DataItemIds Id;
	TBytes Data;
	std::function<void(void *)> _init;
	std::function<void(void *)> _go;
	std::function<void(void *)> _calcPayload;
	std::function<void(void *)> _asString;
	__u8 minlen;
	__u8 typlen;
	__u8 maxlen;
	std::string desc;

	virtual void init(void){/* if _init is not nullptr call it otherwise default behavior */};
	virtual void go(void){/* if _go is not nullptr call it otherwise default behavior */};
	virtual void calcPayload(void){/* if _calcPayload is not nullptr call it, otherwise default behavior */};
	virtual std::string asString(void)
	{ /* if _asString is not nullptr call it othwerise:  */
		return fmt::format("{}->{}", (__u16)Id, (void *)Data.data());
	};

public:
	DataItem(TBytes Serialized)
	{ // TODO: check Serialized is long enough
		// extract one DataItem from "Serialized" including Id and payload
		int ofs = 0;
		memcpy(&Id, Serialized.data() + ofs, sizeof(Id));
		ofs += sizeof(Id);
		TDataItemLength len;
		memcpy(&len, Serialized.data() + ofs, sizeof(len));
		ofs += sizeof(len);
		Data.insert(Data.end(), Serialized.cbegin() + ofs, Serialized.cbegin() + ofs + len);

		auto item = DIdDict2.find(Id);
		if (item == DIdDict2.end())
			throw std::logic_error("DId not found in list");
		// copy list entry into object.  Note: recent change, not used as much as it could/should be
		minlen = item->second.minLen;
		typlen = item->second.typLen;
		maxlen = item->second.maxLen;
		desc = item->second.desc;
		_init = item->second.init;
		_go = item->second.go;
		_calcPayload = item->second.calcPayload;
		_asString = item->second.print;
	}
};


// Base class for all TDataItems, descendants of which will handle payloads specific to the DId
class TDataItem : TDataItemParent
{
public:
	// 1) Deserialization: methods used when converting byte vectors into objects

	// factory fromBytes() instantiates appropriate (sub-)class of TDataItem via DIdList[]
	// .fromBytes() would typically be called by TMessage::fromBytes();
	static PTDataItem fromBytes(TBytes msg, TError &result);

	// this block of methods are typically used by ::fromBytes() to syntax-check the byte vector
	static int validateDataItemPayload(DataItemIds DataItemID, TBytes Data);
	static int isValidDataItemID(DataItemIds DataItemID);
	static int validateDataItem(TBytes msg);
	static TDataItemLength getMinLength(DataItemIds DId);
	static TDataItemLength getTargetLength(DataItemIds DId);
	static TDataItemLength getMaxLength(DataItemIds DId);

	// index into DIdList; TODO: kinda belongs in a DIdList class method...
	static int getDIdIndex(DataItemIds DId);
	// serialize the Payload portion of the Data Item; calling this->calcPayload is done by TDataItem.AsBytes(), only
	virtual TBytes calcPayload(bool bAsReply = false) { return Data; }
	// serialize for sending via TCP; calling TDataItem.AsBytes() is normally done by TMessage::AsBytes()
	virtual TBytes AsBytes(bool bAsReply = false);

	// 2) Serialization: methods for source to generate TDataItems, typically for "Response Messages"

	// zero-"Data" data item constructor
	TDataItem(DataItemIds DId);
	// some-"Data" constructor for specific DId; *RARE*, *DEBUG mainly, to test round-trip conversion implementation*
	// any DId that is supposed to have data would use its own constructor that takes the correct data types, not a
	// simple TBytes, for the Data Payload
	TDataItem(DataItemIds DId, TBytes bytes) : TDataItemParent(DId, bytes)
	{
		Debug("TDataItem(DId,bytes) constructor");
	};

	// TDataItem anItem(DId_Read1).addData(offset) kind of thing.  no idea if it will be useful other than debugging
	virtual TDataItem &addData(__u8 aByte);
	// TDataItem anItem().setDId(DId_Read1) kind of thing.  no idea why it exists
	virtual TDataItem &setDId(DataItemIds DId);
	// encapsulates reading the Id for the DataItem
	virtual DataItemIds getDId();

	virtual bool isValidDataLength();

	// parse byte array into TDataItem; *RARE*, *DEBUG mainly, to test round-trip conversion implementation*
	// this is an explicit class-specific .fromBytes(), which the class method .fromBytes() will invoke for NYI DIds etc
	TDataItem(TBytes bytes);

	// 3) Verbs -- things used when *executing* the TDataItem Object
public:
	// intended to be overriden by descendants it performs the query/config operation and sets instance state as appropriate
	virtual TDataItem &Go();
	// encapsulates the result code of .Go()'s operation
	virtual TError getResultCode();
	// encapsulates the Value that results from .Go()'s operation; DIO_Read1() might have a bool Value;
	// ADC_GetImmediateScanV() might be an array of single precision floating point Volts
	virtual std::shared_ptr<void> getResultValue(); // TODO: fix; think this through

	// 4) Diagnostic / Debug - methods typically used for implementation debugging
public:
	// returns human-readable string representing the DataItem and its payload; normally used by TMessage.AsString(bAsReply)
	virtual std::string AsString(bool bAsReply = false);
	// used by .AsString(bAsReply) to fetch the human-readable name/description of the DId (from DIdList[].Description)
	virtual std::string getDIdDesc();
	// class method to get the human-readable name/description of any known DId; TODO: should maybe be a method of DIdList[]
	static std::string getDIdDesc(DataItemIds DId);

public:
	TError resultCode;
	int conn;

protected:
	bool bWrite = false;
};
#pragma endregion TDataItem declaration

#pragma region "class TDataItemNYI" declaration
class TDataItemNYI : public TDataItem
{
public:
	TDataItemNYI() = default;
	TDataItemNYI(TBytes buf) : TDataItem::TDataItem{buf} {};
};
#pragma endregion
