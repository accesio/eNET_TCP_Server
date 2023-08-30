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
#include <cstring>
#include <iomanip>
#include <linux/types.h>
#include <vector>

#include "eNET-AIO16-16F.h" // widthFromOffset()
#include "utilities.h"
#include "TError.h"

/* type definitions */
using TBytes = std::vector<__u8>;

/* utility functions */
bool sanitizePath(std::string &path);
std::string GetTempFileName();

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
		case 8: return *buf;
		case 16: return *reinterpret_cast<__u16 *> ((buf));
		case 32: return *reinterpret_cast<__u32 *> ((buf));
	}
	return 0;
}

template <typename T>
void stuff(TBytes &buf, const T v)
{
	auto value = v;
	for (uint i = 0; i < sizeof(T); i++)
	{
		buf.push_back(static_cast<__u8>(value & 0xFF));
		value = static_cast<T>(value>>8);
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



// throw exception if conditional is false
inline void
GUARD(bool allGood, TError resultcode, int intInfo,
	  int Line = __builtin_LINE(), const char *File = __builtin_FILE(), const char *Func = __builtin_FUNCTION())
{
	if (!(allGood))
		// throw std::logic_error("GUARD! "+  std::string(File) + ": " + std::string(Func) + "(" + std::to_string(Line) + "): -" + std::to_string(-resultcode) + " = " + to_hex<__u32>(intInfo));
		throw std::logic_error("\nGUARD! " + std::string(File) + ": " + std::string(Func) + "(" + std::to_string(Line) + "): -" + std::to_string(-resultcode) + " " + std::string(err_msg[-resultcode]) + " = " + to_hex<__u32>(intInfo));
}
