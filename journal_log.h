#pragma once

#include <cstdint>
#include <string>

namespace JournalLog
{
enum class Source : std::uint8_t
{
    AioEnetdService = 1,
    Kernel = 2,
    System = 3
};

struct Page
{
    int errorNumber = 0;
    bool more = false;
    bool eof = false;
    bool truncatedEntry = false;
    bool cursorGap = false;
    std::uint16_t entryCount = 0;
    std::uint64_t firstRealtimeUsec = 0;
    std::uint64_t lastRealtimeUsec = 0;
    std::string nextCursor;
    std::string text;
};

Page ReadPage(Source source, bool includePreviousBoots,
              const std::string &cursor, std::uint16_t maxEntries,
              std::uint16_t maxTextBytes) noexcept;
} // namespace JournalLog
