#pragma once

/*

This file is intended to provide standardized type names for use in ACCES' eNET- Series devices.
This improves portability and flexibility; changing the underlying type of a thing is easier if
has a unique type name.
(Frex, we changed from 2-byte to 4-byte TMessage Payload Lengths with only one line in this file)
	(well, plus some bug-fixing, because *that* goal was aspirational.)

This file also has some other crap in it, like some utility stuff and the enum for
TDataItem IDs, but that should change.

*/

#ifdef __linux__
#include <linux/types.h>
#else // define our base types for compiling on Windows
typedef uint8_t __u8;
typedef __u16 __u16;
typedef uint32_t __u32;
#endif

#include <cstring>
#include <string>
#include <iomanip>
#include <memory>
#include <vector>
#include <thread>

#include "safe_queue.h"
#include "TError.h"

/* type definitions */
using TBytes = std::vector<__u8>;
typedef __u8 TMessageId;
typedef __u32 TMessagePayloadSize;
typedef __u8 TCheckSum;
typedef __u16 TDataId;
using TDataItemLength = __u16;
using TDataId = __u16;
using TDataItemLength=__u16;


template<typename> class TDataItem;

#pragma region TDataItem DId enum

#define _INVALID_DATAITEMID_ ((TDataId)-2)

enum class DataItemIds : TDataId
{
	INVALID = _INVALID_DATAITEMID_,
	// Note 1: the "TLA_" DIds (e.g., `BRD_` and `REG_` et al; i.e., those DId names that don't have anything after the `_`)
	//         return human-readable text of all TLA_ category DIds and what they do & why
	BRD_ = 0x0000, // Query Only.
	BRD_Reset = 0x0001,
	BRD_DeviceID = 0x0002,
	BRD_Features = 0x0003,
	BRD_FpgaID = 0x0004,
	BRD_Model = 0x0008,
	BRD_GetModel = 0x0009,
	BRD_SerialNumber = 0x10,
	BRD_GetSerialNumber = 0x11,
	BRD_NumberOfSubmuxes = 0x21,
	BRD_SubmuxScale = 0x23,
	BRD_SubmuxOffset = 0x24,
	BRD_GetNumberOfSubmuxes = 0x31,
	BRD_GetSubmuxScale = 0x33,
	BRD_GetSubmuxOffset = 0x34,
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

	REG_ReadBit = 0x121,
	REG_WriteBit,
	REG_ClearBit,
	REG_SetBit,
	REG_ToggleBit,

	DAC_ = 0x200, // Query Only. *1
	DAC_Output1 = 0x201,
	DAC_OutputAll = 0x202,
	DAC_Output1V = 0x203,
	DAC_Range1 = 0x204, // Query Only.
	DAC_Calibrate1 = 0x20C,
	DAC_CalibrateAll,
	DAC_Offset1 = 0x20E,
	DAC_OffsetAll = 0x20F,
	DAC_Scale1 = 0x210,
	DAC_ScaleAll = 0x211,
	DAC_Configure1 = 0x2C0,
	DAC_ConfigAndOutput1 = 0x2C1,

	DIO_ = 0x300, // Query Only. *1
	DIO_ConfigureBit,
	DIO_Configure,
	DIO_InputBit,
	DIO_Input = 0x0305,
	DIO_InputBuf1,
	DIO_InputBufAll,
	DIO_OutputBit,
	DIO_Output,
	DIO_OutputBuf, // like unpaced waveform output; NOTE: not sure this is useful
	DIO_ConfigureReadWriteReadSome,
	DIO_ClearBit,
	DIO_ClearAll,
	DIO_SetBit,
	DIO_SetAll,
	DIO_SetSome,
	DIO_ToggleBit,
	DIO_ToggleAll,
	DIO_PulseBit,
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
	CFG_Hostname = 0x9001,
	SYS_UploadFileName = 0xEF01,
	SYS_UploadFileData = 0xEF02,
	DOC_Get = 0xFFFF,
};
#pragma endregion

// convert integer to hex, no '0x' prefixed
// template <typename T>
// inline std::string to_hex(T i)
// {
// 	// Ensure this function is called with a template parameter that makes sense. Note: static_assert is only available in C++11 and higher.
// 	static_assert(std::is_integral<T>::value, "Template argument 'T' must be a fundamental integer type (e.g. int, short, etc..).");

// 	std::stringstream stream;
// 	stream << /*std::string("0x") <<*/ std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex;

// 	// If T is an 8-bit integer type (e.g. uint8_t or int8_t) it will be
// 	// treated as an ASCII code, giving the wrong result. So we use C++17's
// 	// "if constexpr" to have the compiler decides at compile-time if it's
// 	// converting an 8-bit int or not.
// 	if constexpr (std::is_same_v<std::uint8_t, T>)
// 	{
// 		// Unsigned 8-bit unsigned int type. Cast to int (thanks Lincoln) to
// 		// avoid ASCII code interpretation of the int. The number of hex digits
// 		// in the  returned string will still be two, which is correct for 8 bits,
// 		// because of the 'sizeof(T)' above.
// 		stream << static_cast<int>(i);
// 	}
// 	else if (std::is_same_v<std::int8_t, T>)
// 	{
// 		// For 8-bit signed int, same as above, except we must first cast to unsigned
// 		// int, because values above 127d (0x7f) in the int will cause further issues.
// 		// if we cast directly to int.
// 		stream << static_cast<int>(static_cast<uint8_t>(i));
// 	}
// 	else
// 	{
// 		// No cast needed for ints wider than 8 bits.
// 		stream << i;
// 	}

// 	return stream.str();
// }

#pragma pack(push, 1)
typedef struct {
	__u8 offset;
	__u8 width;
	__u32 value;
} REG_Write;

typedef std::vector<REG_Write> REG_WriteList;

typedef struct
{
	TMessageId type;
	TMessagePayloadSize payload_size;
} TMessageHeader;


typedef struct TSendQueueItemClass
{
	// which TCP-per-client-read thread put this item into the Action Queue
	pthread_t &receiver;
	// which thread is responsible for sending results of the action to the client
	pthread_t &sender;
	// which queue is the sender-thread popping from
	SafeQueue<TSendQueueItemClass> &sendQueue;
	// which client is all this from/for
	int clientref;
	// what TCP port# was this received on
	int portReceive;
	// what TCP port# is this sending out on
	int portSend;
} TSendQueueItem;

#pragma pack(pop)

// throw exception if conditional is false
// inline void
// GUARD(bool allGood, TError resultcode, int intInfo,
// 	  int Line = __builtin_LINE(), const char *File = __builtin_FILE(), const char *Func = __builtin_FUNCTION())
// {
// 	if (!(allGood))
// 		throw std::logic_error(std::string(File) + ": " + std::string(Func) + "(" + std::to_string(Line) + "): " + std::to_string(-resultcode) + " = " + std::to_string(intInfo));
// }

