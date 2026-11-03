// Fuzzes the Wavefront OBJ parser (src/libslic3r/Format/objparser.cpp), stream entry point.
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "libslic3r/Format/objparser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::istringstream in(std::string(reinterpret_cast<const char *>(data), size));
    ObjParser::ObjData obj;
    ObjParser::objparse(in, obj);
    return 0;
}
