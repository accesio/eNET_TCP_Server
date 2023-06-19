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

#include <linux/types.h>
#include <cstring>
#include <string>
#include <iomanip>
#include <memory>
#include <vector>
#include <thread>

#include "eNET-AIO16-16F.h"
#include "safe_queue.h"
#include "TError.h"



/* type definitions */
typedef std::vector<__u8> TBytes;
typedef __u8 TMessageId;
typedef __u32 TMessagePayloadSize;
typedef __u8 TCheckSum;
typedef __u16 TDataId;
typedef __u16 TDataItemLength;

class TDataItem;

typedef std::shared_ptr<TDataItem> PTDataItem;
typedef std::vector<PTDataItem> TPayload;

template<typename To, typename From>
To bit_cast(const From& from) {
    static_assert(sizeof(From) == sizeof(To),
                  "Source and destination types must have the same size for bit_cast.");
    To to;
    std::memcpy(&to, &from, sizeof(from));
    return to;
}

template <typename T>
std::vector<T> slicing(std::vector<T> const &v, int Start, int End)
{
	// Begin and End iterator
	auto first = v.begin() + Start;
	auto last = v.begin() + End + 1;

	// Copy the element
	std::vector<T> vector(first, last);

	// Return the results
	return vector;
}

// convert integer to hex, no '0x' prefixed
template <typename T>
inline std::string to_hex(T i)
{
	// Ensure this function is called with a template parameter that makes sense. Note: static_assert is only available in C++11 and higher.
	static_assert(std::is_integral<T>::value, "Template argument 'T' must be a fundamental integer type (e.g. int, short, etc..).");

	std::stringstream stream;
	stream << /*std::string("0x") <<*/ std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex;

	// If T is an 8-bit integer type (e.g. uint8_t or int8_t) it will be
	// treated as an ASCII code, giving the wrong result. So we use C++17's
	// "if constexpr" to have the compiler decides at compile-time if it's
	// converting an 8-bit int or not.
	if constexpr (std::is_same_v<std::uint8_t, T>)
	{
		// Unsigned 8-bit unsigned int type. Cast to int (thanks Lincoln) to
		// avoid ASCII code interpretation of the int. The number of hex digits
		// in the  returned string will still be two, which is correct for 8 bits,
		// because of the 'sizeof(T)' above.
		stream << static_cast<int>(i);
	}
	else if (std::is_same_v<std::int8_t, T>)
	{
		// For 8-bit signed int, same as above, except we must first cast to unsigned
		// int, because values above 127d (0x7f) in the int will cause further issues.
		// if we cast directly to int.
		stream << static_cast<int>(static_cast<uint8_t>(i));
	}
	else
	{
		// No cast needed for ints wider than 8 bits.
		stream << i;
	}

	return stream.str();
}

#define printBytes(dest, intro, buf, crlf)                                                                           \
	{                                                                                                                \
		dest << intro;                                                                                               \
		for (auto byt : buf)                                                                                         \
			dest << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byt) << " "; \
		if (crlf)                                                                                                    \
			dest << '\n';                                                                                       \
	}

inline __u32 regextract(__u8 * buf, int regofs)
{
	switch(widthFromOffset(regofs))
	{
		case 8: return *(__u8 *)(buf);
		case 16: return *(__u16 *)(buf);
		case 32: return *(__u32 *)(buf);
	}
	return 0;
}

template <typename T>
void stuff(TBytes &buf, const T v)
{
	auto value = v;
	for (int i = 0; i < sizeof(T); i++)
	{
		buf.push_back(value & 0xFF);
		value >>= 8;
	}
}

template <>
inline void stuff<std::string>(TBytes &buf, const std::string v)
{
	for (char c : v)
		buf.push_back(c);
}

template <>
inline void stuff<float>(TBytes &buf, const float v)
{
	union
	{
		__u32 u;
		float f;
	} map_f__u32;
	map_f__u32.f = v;
	stuff<__u32>(buf, map_f__u32.u);
}


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



extern const char *err_msg[];

// throw exception if conditional is false
inline void
GUARD(bool allGood, TError resultcode, int intInfo,
	  int Line = __builtin_LINE(), const char *File = __builtin_FILE(), const char *Func = __builtin_FUNCTION())
{
	if (!(allGood))
		// throw std::logic_error("GUARD! "+  std::string(File) + ": " + std::string(Func) + "(" + std::to_string(Line) + "): -" + std::to_string(-resultcode) + " = " + to_hex<__u32>(intInfo));
		throw std::logic_error("\nGUARD! " + std::string(File) + ": " + std::string(Func) + "(" + std::to_string(Line) + "): -" + std::to_string(-resultcode) + " " + std::string(err_msg[-resultcode]) + " = " + to_hex<__u32>(intInfo));
}
