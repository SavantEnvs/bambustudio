// Fuzzes the G-code line parser/tokenizer (src/libslic3r/GCodeReader.cpp) on an in-memory buffer,
// the way the G-code preview and post-processors drive it.
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>
#include <string>

#include "libslic3r/GCodeReader.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string buf(reinterpret_cast<const char *>(data), size);
    Slic3r::GCodeReader reader;
    double sink = 0;
    reader.parse_buffer(buf, [&sink](Slic3r::GCodeReader &r, const Slic3r::GCodeReader::GCodeLine &line) {
        if (line.has_x()) sink += line.x();
        if (line.has_y()) sink += line.y();
        if (line.has_z()) sink += line.z();
        if (line.has_e()) sink += line.e();
        if (line.has_f()) sink += line.f();
        sink += line.dist_XY(r) + line.dist_E(r);
        sink += line.extruding(r) + line.retracting(r) + line.travel();
        float v;
        for (char axis : {'S', 'P', 'T', 'M', 'I', 'J'})
            if (line.has_value(axis, v))
                sink += v;
        (void) line.cmd();
        (void) line.comment();
    });
    Slic3r::GCodeReader single;
    single.parse_line(buf, [&sink](Slic3r::GCodeReader &r, const Slic3r::GCodeReader::GCodeLine &line) {
        sink += line.dist_XY(r) + line.dist_E(r) + double(line.extruding(r));
        (void) line.cmd();
        (void) line.comment();
    });
    return sink == 12345.678 ? 1 : 0;
}
