// Fuzzes BambuStudio's settings-search fuzzy matcher (src/slic3r/GUI/fts_fuzzy_match.h) the way
// GUI/Search.cpp drives it: a wide pattern matched against a wide label, with a fixed-size match buffer.
// Input: "<pattern>\n<label>", both UTF-8.
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>
#include <string>

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "slic3r/GUI/fts_fuzzy_match.h"

static std::wstring utf8_to_wide(const uint8_t *p, size_t n)
{
    std::wstring out;
    for (size_t i = 0; i < n;) {
        uint32_t c = p[i];
        size_t   len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
        if (len == 1 || i + len > n) {
            out.push_back(wchar_t(c & 0x7f ? c : 0x20));
            ++i;
            continue;
        }
        c &= 0xff >> (len + 1);
        for (size_t k = 1; k < len; ++k)
            c = (c << 6) | (p[i + k] & 0x3f);
        out.push_back(c ? wchar_t(c) : wchar_t(0x20));
        i += len;
    }
    return out;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    size_t split = 0;
    while (split < size && data[split] != '\n')
        ++split;
    if (split == size)
        return -1;
    std::wstring pattern = utf8_to_wide(data, split);
    std::wstring label   = utf8_to_wide(data + split + 1, size - split - 1);

    uint16_t matches[fts::max_matches + 1];
    int      score = 0;
    if (fts::fuzzy_match(pattern.c_str(), label.c_str(), score, matches)) {
        size_t cnt = 0;
        for (; matches[cnt] != fts::stopper; ++cnt)
            if (matches[cnt] >= label.size())
                __builtin_trap();
        (void) cnt;
    }
    return 0;
}
