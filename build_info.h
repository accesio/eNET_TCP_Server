#pragma once

namespace BuildInfo
{
extern const unsigned Major;
extern const unsigned Minor;
extern const unsigned Patch;
extern const unsigned Build;

// "major.minor.patch.build"
extern const char Version[];
// Product name followed by Version.
extern const char ProductVersion[];
extern const char GitHash[];
extern const char GitDescribe[];
// UTC ISO-8601 build timestamp, and its date/time components.
extern const char BuildUtc[];
extern const char BuildDate[];
extern const char BuildTime[];
} // namespace BuildInfo
