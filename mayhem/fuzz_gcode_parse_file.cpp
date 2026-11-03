// Fuzzes the chunked G-code file reader (GCodeReader::parse_file / parse_file_raw in
// src/libslic3r/GCodeReader.cpp), which is what loads user G-code files into the previewer.
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>
#include <string>

#include "libslic3r/GCodeReader.hpp"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ScratchFile f(data, size);
    if (!f.ok)
        return 0;
    size_t lines = 0;
    {
        Slic3r::GCodeReader reader;
        std::vector<size_t> ends;
        reader.parse_file(f.path, [&lines](Slic3r::GCodeReader &r, const Slic3r::GCodeReader::GCodeLine &line) {
            ++lines;
            (void) line.extruding(r);
            (void) line.cmd();
        }, ends);
    }
    {
        Slic3r::GCodeReader reader;
        reader.parse_file_raw(f.path, [&lines](Slic3r::GCodeReader &, const char *b, const char *e) { lines += size_t(e - b); });
    }
    return lines == size_t(-1) ? 1 : 0;
}
