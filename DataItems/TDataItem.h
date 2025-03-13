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
#include <memory>
#include <iterator>
#include <fmt/core.h>

#include "../logging.h"
#include "../utilities.h"
#include "../eNET-AIO16-16F.h"
#include "../TError.h"
#include "../apci.h"

class TDataItemBase;

using PTDataItem = std::shared_ptr<TDataItemBase>;
using TPayload = std::vector<PTDataItem>;
using TDataId = __u16;
using TDataItemLength=__u16;

#pragma region TDataItem DId enum

#define _INVALID_DATAITEMID_ ((TDataId)-1)

enum class DataItemIds : TDataId
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
	SYS_UploadFileName=0xEF01,
	SYS_UploadFileData=0xEF02,
};
#pragma endregion

#pragma pack(push, 1)

int validateDataItemPayload(DataItemIds DataItemID, TBytes Data);
// return register width for given offset as defined for eNET-AIO registers
// returns 0 if offset is invalid
int widthFromOffset(int ofs);

#pragma region inserting ancestor under TDataItem
using PTDataItemBase = std::shared_ptr<class TDataItemBase>;
class TDataItemBase
{
public:
    // ========== Common Fields ==========
	TBytes Data;
    DataItemIds DId;      // e.g., DAC_Output1, DAC_Range1
    TError resultCode = ERR_SUCCESS;
    bool bWrite = false;  // Used by many items to indicate write vs. read
    int conn = 0;         // Connection ID or similar
    // ========== Constructors ==========
    explicit TDataItemBase(DataItemIds dId)
        : DId(dId)
    {
        Debug("TDataItemBase(DId) constructor");
    }

    virtual ~TDataItemBase() {}

    // ========== Virtual Methods for Polymorphism ==========
    // Called by the main worker thread to execute hardware logic
    virtual TDataItemBase &Go() = 0;

    // Serializes the data item to bytes (for sending over TCP)
    virtual TBytes calcPayload(bool bAsReply = false) = 0;

    // High-level string form for debugging/logging
    virtual std::string AsString(bool bAsReply = false) = 0;

    // ========== Static / Factory Methods ==========
    static PTDataItemBase fromBytes(const TBytes &msg, TError &result);

    static int validateDataItemPayload(DataItemIds DataItemID, const TBytes &Data);
    static int isValidDataItemID(DataItemIds DataItemID);
    static int validateDataItem(const TBytes &msg);
    static __u16 getMinLength(DataItemIds DId);
    static __u16 getTargetLength(DataItemIds DId);
    static __u16 getMaxLength(DataItemIds DId);
    static int getDIdIndex(DataItemIds DId);
    TDataItemBase &addData(__u8 aByte);
    TDataItemBase &setDId(DataItemIds newId);
    DataItemIds getDId() const;
    bool isValidDataLength() const;
    TBytes AsBytes(bool bAsReply);
    std::string getDIdDesc() const;
    TError getResultCode();
    std::shared_ptr<void> getResultValue();
    // These might be used for name lookups, etc.
    static std::string getDIdDesc(DataItemIds DId);
};
#pragma endregion

typedef std::shared_ptr<TDataItemBase> DIdConstructor(DataItemIds DId, TBytes FromBytes);

template <class q>
std::shared_ptr<TDataItemBase> construct(DataItemIds DId, TBytes FromBytes)
{
    // q must inherit from TDataItemBase
    return std::make_shared<q>(DId, FromBytes);
}


typedef struct
{
	DataItemIds DId;
	TDataItemLength dataLength;
} TDataItemHeader;

typedef struct __DIdDictEntry_inner
{
	TDataItemLength minLen;
	TDataItemLength typLen;
	TDataItemLength maxLen;
	// std::function<std::unique_ptr<TDataItem>(TBytes)> Construct;
	DIdConstructor *Construct;
	std::string desc;
	std::function<void(void *)> go;
} TDIdDictEntry;

//  __DIdDictEntry_inner ;
extern const std::map<DataItemIds, TDIdDictEntry> DIdDict;



#pragma region "class TDataItem" declaration


// Base class for all TDataItems, descendants of which will handle payloads specific to the DId
template <typename ParamStruct>
class TDataItem : public TDataItemBase
{
public:
    ParamStruct params;  // e.g., DAC_OutputParams

    // We keep a reference to the raw bytes, too, if you need them
    // (In case you want to parse in a custom way.)
    TBytes rawBytes;

    // Constructor
    TDataItem(DataItemIds dId, const TBytes &bytes)
        : TDataItemBase(dId),
          rawBytes(bytes)
    {
        Debug("TDataItem<ParamStruct>(dId, bytes) constructor");

        // By default, copy entire `bytes` into `params` if it fits
        if (bytes.size() >= sizeof(ParamStruct)) {
            std::memcpy(&params, bytes.data(), sizeof(ParamStruct));
        }
        else {
            // We'll let the derived class handle partial or custom parsing if it wants
            Debug("ParamStruct is bigger than rawBytes; derived constructor may fix this.");
        }
    }

    virtual ~TDataItem() {}

    // By default, we serialize `params` back to bytes
    virtual TBytes calcPayload(bool bAsReply = false) override
    {
        TBytes out;
        out.insert(out.end(),
                   reinterpret_cast<const __u8*>(&params),
                   reinterpret_cast<const __u8*>(&params) + sizeof(ParamStruct));
        return out;
    }

    // Must implement
    virtual TDataItemBase &Go() override = 0;
    virtual std::string AsString(bool bAsReply = false) override = 0;
};

#pragma endregion TDataItem declaration

#pragma region "class TDataItemNYI" declaration
struct NYIParams {
    // no fields
};

class TDataItemNYI : public TDataItem<NYIParams>
{
public:
    // Must match the TDataItem<NYIParams>(DataItemIds dId, TBytes bytes) signature
    TDataItemNYI(DataItemIds dId, TBytes bytes)
        : TDataItem(dId, bytes)
    {
        // Possibly do debug logs
        Debug("TDataItemNYI constructor: DId=" + std::to_string((int)DId));
    }

    // Implement pure virtuals from TDataItemBase
    TDataItemBase &Go() override {
        // do nothing or debug
        Debug("NYI Go()");
        return *this;
    }

    std::string AsString(bool bAsReply = false) override {
        return "NYI DataItem(DId=" + std::to_string((int)DId) + ")";
    }
};

#pragma endregion
