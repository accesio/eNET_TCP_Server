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
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <linux/types.h>
#include <stdexcept>
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
    static_assert(std::is_integral<T>::value,
                  "Template argument 'T' must be an integral type.");

    // We want two’s-complement representation for negative values,
    // so we must copy the *bit pattern* to an unsigned type of the same size.
    // This ensures we get the “raw” bits, including sign bit for negative ints.
    using U = std::make_unsigned_t<T>;
    U bits;
    static_assert(sizeof(U) == sizeof(T), "Size mismatch?!");

    // Copy the bytes directly (including sign bit).
    std::memcpy(&bits, &i, sizeof(T));

    // We'll produce minimal hex into buffer, then zero-pad to 2×sizeof(T).
    char buffer[2 * sizeof(T) + 1]; // +1 for the null terminator
    auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), bits, 16);
    if (ec != std::errc())
    {
        // Should never happen unless 'bits' is extremely large (it isn't).
        throw std::runtime_error("std::to_chars failed in to_hex<T>.");
    }

    // 'ptr' points to the end of the written string (not null-terminated).
    // Let's see how many chars we wrote.
    int length = static_cast<int>(ptr - buffer);
    // We want exactly 2×sizeof(T) hex digits, e.g. 2 for 8-bit, 4 for 16-bit, etc.
    int needed = static_cast<int>(2 * sizeof(T));

    // Build an output string with (needed - length) leading '0'.
    // Then append the actual hex digits from buffer.
    std::string out(std::max(0, needed - length), '0');
    out.append(buffer, length);

    // Convert to uppercase (since to_chars uses lowercase by default)
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return static_cast<char>(std::toupper(c)); });

    return out;
}


// Overload of to_hex() for TBytes
inline std::string to_hex(const TBytes &bytes, bool spaced = true)
{
    if (bytes.empty()) return "";

    std::stringstream stream;
    stream << std::hex << std::setfill('0') << std::uppercase;

    // Convert each byte to two-digit hex.
    // Insert spaces if 'spaced' is true.
    for (size_t i = 0; i < bytes.size(); i++)
    {
        // Print each byte as two hex digits
        stream << std::setw(2) << static_cast<int>(bytes[i]);

        // Optionally add a space except after the last byte
        if (spaced && i + 1 < bytes.size())
            stream << ' ';
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

// return the 8-, 16-, or 32-bit value pointed to by buf based on the register offset
// note: 16- is unused in the ETH-AIO Family
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

std::vector<std::string> split(const std::string &s, char delim);