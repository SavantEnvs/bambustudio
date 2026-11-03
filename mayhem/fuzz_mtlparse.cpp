// Fuzzes the Wavefront MTL material-library parser (src/libslic3r/Format/objparser.cpp), path entry point.
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>

#include "libslic3r/Format/objparser.hpp"
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ScratchFile f(data, size);
    if (!f.ok)
        return 0;
    ObjParser::MtlData mtl;
    ObjParser::mtlparse(f.path.c_str(), mtl);
    return 0;
}
