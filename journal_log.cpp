#include "journal_log.h"

#include <systemd/sd-id128.h>
#include <systemd/sd-journal.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace JournalLog
{
namespace
{
constexpr std::uint16_t DefaultEntries = 128;
constexpr std::uint16_t MaximumEntries = 512;
constexpr std::uint16_t DefaultTextBytes = 48U * 1024U;
constexpr std::uint16_t MinimumTextBytes = 256;
constexpr std::size_t MaximumFormattedEntry = 8192;

using JournalPtr = std::unique_ptr<sd_journal, decltype(&sd_journal_close)>;

struct Entry
{
    std::uint64_t usec = 0;
    std::string cursor;
    std::string text;
};

std::string Sanitize(std::string value)
{
    std::string clean;
    clean.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char u = static_cast<unsigned char>(value[i]);

        // aioenetd currently emits ANSI SGR color sequences on stdout.  Strip
        // CSI escape sequences so journal pages contain plain journalctl-like
        // text rather than fragments such as "[31m".
        if (u == 0x1BU && i + 1U < value.size() && value[i + 1U] == '[')
        {
            i += 2U;
            while (i < value.size())
            {
                const unsigned char c = static_cast<unsigned char>(value[i]);
                if (c >= 0x40U && c <= 0x7EU)
                    break;
                ++i;
            }
            continue;
        }

        if ((u < 0x20U && value[i] != '\t') || u == 0x7FU)
            clean.push_back(' ');
        else
            clean.push_back(value[i]);
    }
    return clean;
}

std::string GetField(sd_journal *journal, const char *field)
{
    const void *raw = nullptr;
    std::size_t length = 0;
    const int rc = sd_journal_get_data(journal, field, &raw, &length);
    if (rc < 0 || raw == nullptr)
        return {};

    const std::string_view data(static_cast<const char *>(raw), length);
    const std::string prefix = std::string(field) + "=";
    if (data.size() < prefix.size() || data.substr(0, prefix.size()) != prefix)
        return {};
    return Sanitize(std::string(data.substr(prefix.size())));
}

std::string FormatTimestamp(std::uint64_t usec)
{
    const std::time_t seconds = static_cast<std::time_t>(usec / 1000000ULL);
    std::tm tm{};
    if (gmtime_r(&seconds, &tm) == nullptr)
        return "0000-00-00T00:00:00.000000Z";

    std::array<char, 96> buffer{};
    const unsigned micros = static_cast<unsigned>(usec % 1000000ULL);
    std::snprintf(buffer.data(), buffer.size(),
                  "%04d-%02d-%02dT%02d:%02d:%02d.%06uZ",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec, micros);
    return buffer.data();
}

std::string CurrentCursor(sd_journal *journal)
{
    char *cursor = nullptr;
    if (sd_journal_get_cursor(journal, &cursor) < 0 || cursor == nullptr)
        return {};
    std::string result(cursor);
    std::free(cursor);
    return result;
}

std::string FormatCurrentEntry(sd_journal *journal, Source source,
                               std::uint64_t usec, bool &truncated)
{
    std::string identifier = GetField(journal, "SYSLOG_IDENTIFIER");
    if (identifier.empty())
        identifier = GetField(journal, "_COMM");
    if (identifier.empty())
    {
        switch (source)
        {
        case Source::AioEnetdService: identifier = "aioenetd"; break;
        case Source::Kernel: identifier = "kernel"; break;
        case Source::System: identifier = "journal"; break;
        }
    }

    const std::string pid = GetField(journal, "_PID");
    const std::string priority = GetField(journal, "PRIORITY");
    std::string message = GetField(journal, "MESSAGE");
    if (message.empty())
        message = "(no MESSAGE field)";

    std::string line = FormatTimestamp(usec) + " <" +
                       (priority.empty() ? "?" : priority) + "> " + identifier;
    if (!pid.empty())
        line += "[" + pid + "]";
    line += ": " + message + "\n";

    if (line.size() > MaximumFormattedEntry)
    {
        line.resize(MaximumFormattedEntry);
        line.back() = '\n';
        truncated = true;
    }
    return line;
}

int AddCurrentBootMatch(sd_journal *journal)
{
    sd_id128_t bootId{};
    int rc = sd_id128_get_boot(&bootId);
    if (rc < 0)
        return rc;

    std::array<char, SD_ID128_STRING_MAX> id{};
    sd_id128_to_string(bootId, id.data());
    const std::string match = "_BOOT_ID=" + std::string(id.data());
    return sd_journal_add_match(journal, match.data(), match.size());
}

// Leave the journal positioned immediately after the continuation boundary.
// The first sd_journal_previous() therefore returns the next older entry.
int Position(sd_journal *journal, const std::string &cursor, bool &cursorGap)
{
    if (cursor.empty())
        return sd_journal_seek_tail(journal);

    int rc = sd_journal_seek_cursor(journal, cursor.c_str());
    if (rc < 0)
        return rc;

    rc = sd_journal_next(journal);
    if (rc < 0)
        return rc;
    if (rc == 0)
    {
        cursorGap = true;
        return sd_journal_seek_tail(journal);
    }

    const int exact = sd_journal_test_cursor(journal, cursor.c_str());
    if (exact < 0)
        return exact;
    cursorGap = exact == 0;
    return 0;
}

int StepPrevious(sd_journal *journal, bool &hasEntry)
{
    const int rc = sd_journal_previous(journal);
    if (rc < 0)
        return rc;
    hasEntry = rc > 0;
    return 0;
}

Page ReadPageImpl(Source source, bool includePreviousBoots,
                  const std::string &cursor, std::uint16_t maxEntries,
                  std::uint16_t maxTextBytes)
{
    Page page;
    if (source != Source::AioEnetdService && source != Source::Kernel &&
        source != Source::System)
    {
        page.errorNumber = EINVAL;
        return page;
    }

    if (maxEntries == 0)
        maxEntries = DefaultEntries;
    maxEntries = std::min(maxEntries, MaximumEntries);
    if (maxTextBytes == 0)
        maxTextBytes = DefaultTextBytes;
    maxTextBytes = std::max(maxTextBytes, MinimumTextBytes);
    maxTextBytes = std::min(maxTextBytes, DefaultTextBytes);

    sd_journal *rawJournal = nullptr;
    int rc = sd_journal_open(&rawJournal,
                             SD_JOURNAL_LOCAL_ONLY | SD_JOURNAL_SYSTEM);
    if (rc < 0)
    {
        page.errorNumber = -rc;
        return page;
    }
    JournalPtr journal(rawJournal, &sd_journal_close);
    (void)sd_journal_set_data_threshold(journal.get(),
                                        MaximumFormattedEntry + 256U);

    const char *sourceMatch = nullptr;
    if (source == Source::AioEnetdService)
        sourceMatch = "_SYSTEMD_UNIT=aioenetd.service";
    else if (source == Source::Kernel)
        sourceMatch = "_TRANSPORT=kernel";

    if (sourceMatch != nullptr)
    {
        rc = sd_journal_add_match(journal.get(), sourceMatch, 0);
        if (rc < 0)
        {
            page.errorNumber = -rc;
            return page;
        }
    }
    if (!includePreviousBoots)
    {
        rc = AddCurrentBootMatch(journal.get());
        if (rc < 0)
        {
            page.errorNumber = -rc;
            return page;
        }
    }

    rc = Position(journal.get(), cursor, page.cursorGap);
    if (rc < 0)
    {
        page.errorNumber = -rc;
        return page;
    }

    std::vector<Entry> entries;
    entries.reserve(maxEntries);
    std::size_t textBytes = 0;
    bool reachedOldest = false;
    bool hasUnreturnedEntry = false;

    while (entries.size() < maxEntries)
    {
        bool hasEntry = false;
        rc = StepPrevious(journal.get(), hasEntry);
        if (rc < 0)
        {
            page.errorNumber = -rc;
            return page;
        }
        if (!hasEntry)
        {
            reachedOldest = true;
            break;
        }

        std::uint64_t usec = 0;
        rc = sd_journal_get_realtime_usec(journal.get(), &usec);
        if (rc < 0)
        {
            page.errorNumber = -rc;
            return page;
        }

        bool entryTruncated = false;
        std::string line = FormatCurrentEntry(journal.get(), source, usec,
                                              entryTruncated);
        std::string entryCursor = CurrentCursor(journal.get());
        if (entryCursor.empty())
        {
            page.errorNumber = EIO;
            return page;
        }

        const std::size_t remaining =
            static_cast<std::size_t>(maxTextBytes) - textBytes;
        if (line.size() > remaining)
        {
            if (!entries.empty())
            {
                // This current entry was not consumed.  Seeking to the cursor of
                // the oldest returned entry will expose it on the next page.
                hasUnreturnedEntry = true;
                break;
            }
            line.resize(remaining);
            if (!line.empty())
                line.back() = '\n';
            entryTruncated = true;
        }

        textBytes += line.size();
        page.truncatedEntry = page.truncatedEntry || entryTruncated;
        entries.push_back(Entry{usec, std::move(entryCursor), std::move(line)});

        if (textBytes >= maxTextBytes)
            break;
    }

    // Determine whether another older matching entry remains, without relying
    // on the page-size limit alone (which would produce a spurious empty page
    // when a page happened to end exactly at EOF).
    if (!reachedOldest && !hasUnreturnedEntry &&
        (entries.size() >= maxEntries || textBytes >= maxTextBytes))
    {
        bool hasEntry = false;
        rc = StepPrevious(journal.get(), hasEntry);
        if (rc < 0)
        {
            page.errorNumber = -rc;
            return page;
        }
        hasUnreturnedEntry = hasEntry;
        reachedOldest = !hasEntry;
    }

    page.more = hasUnreturnedEntry;
    page.eof = reachedOldest && !page.more;
    page.entryCount = static_cast<std::uint16_t>(entries.size());

    if (!entries.empty())
    {
        // The read direction is newest-to-oldest.  The continuation cursor is
        // the oldest returned entry, while text is presented chronologically.
        page.nextCursor = entries.back().cursor;
        page.firstRealtimeUsec = entries.back().usec;
        page.lastRealtimeUsec = entries.front().usec;
        std::reverse(entries.begin(), entries.end());
        page.text.reserve(textBytes);
        for (const Entry &entry : entries)
            page.text += entry.text;
    }
    return page;
}
} // namespace

Page ReadPage(Source source, bool includePreviousBoots,
              const std::string &cursor, std::uint16_t maxEntries,
              std::uint16_t maxTextBytes) noexcept
{
    try
    {
        return ReadPageImpl(source, includePreviousBoots, cursor,
                            maxEntries, maxTextBytes);
    }
    catch (const std::bad_alloc &)
    {
        Page page;
        page.errorNumber = ENOMEM;
        return page;
    }
    catch (...)
    {
        Page page;
        page.errorNumber = EIO;
        return page;
    }
}
} // namespace JournalLog
