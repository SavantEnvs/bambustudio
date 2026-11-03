// Known-answer functional test over the same BambuStudio code the fuzz targets exercise. Prints one
// "key=value" line per check; mayhem/test.sh diffs the output against mayhem/selftest.golden.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "quiet_log.h"
#include <admesh/stl.h>
#include "libslic3r/Format/objparser.hpp"
#include "libslic3r/GCodeReader.hpp"

#define FTS_FUZZY_MATCH_IMPLEMENTATION
#include "slic3r/GUI/fts_fuzzy_match.h"

static std::string write_tmp(const std::string &content)
{
    const char *dir = getenv("TMPDIR");
    std::string path = std::string(dir && *dir ? dir : "/tmp") + "/bbs_selftest_XXXXXX";
    int fd = mkstemp(&path[0]);
    if (fd < 0)
        return "";
    size_t off = 0;
    while (off < content.size()) {
        ssize_t n = write(fd, content.data() + off, content.size() - off);
        if (n <= 0)
            break;
        off += size_t(n);
    }
    close(fd);
    return path;
}

static std::wstring w(const char *s) { return std::wstring(s, s + strlen(s)); }

static std::string unit_cube_stl()
{
    // 8 corners, 12 triangles, outward-facing, CCW.
    static const float v[8][3] = {{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const int   t[12][3] = {{0,2,1},{0,3,2},{4,5,6},{4,6,7},{0,1,5},{0,5,4},
                                   {1,2,6},{1,6,5},{2,3,7},{2,7,6},{3,0,4},{3,4,7}};
    std::string s(80, ' ');
    uint32_t n = 12;
    s.append(reinterpret_cast<const char *>(&n), 4);
    for (auto &tri : t) {
        float rec[12] = {0, 0, 0};
        for (int k = 0; k < 3; ++k)
            memcpy(rec + 3 + 3 * k, v[tri[k]], sizeof(float) * 3);
        s.append(reinterpret_cast<const char *>(rec), sizeof(rec));
        s.append(2, '\0');
    }
    return s;
}

int main()
{
    // fuzzy matcher
    {
        int score = 0;
        uint16_t m[fts::max_matches + 1];
        bool ok = fts::fuzzy_match(w("lyrh").c_str(), w("Layer height").c_str(), score, m);
        printf("fts.layer_height.matched=%d\n", ok);
        printf("fts.layer_height.score=%d\n", score);
        size_t cnt = 0;
        std::string idx;
        for (; m[cnt] != fts::stopper; ++cnt)
            idx += std::to_string(m[cnt]) + ",";
        printf("fts.layer_height.matches=%s\n", idx.c_str());
        int s2 = 0;
        printf("fts.no_match.matched=%d\n", fts::fuzzy_match(w("zzq").c_str(), w("Layer height").c_str(), s2, m));
    }
    // OBJ
    {
        std::istringstream in("o tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\nvt 0.5 0.25\nvn 0 0 1\n"
                              "usemtl red\nf 1/1/1 2/1/1 3/1/1\nf 2 4 3\n");
        ObjParser::ObjData d;
        bool ok = ObjParser::objparse(in, d);
        printf("obj.ok=%d\n", ok);
        printf("obj.coordinate_floats=%zu\n", d.coordinates.size());
        printf("obj.face_vertex_entries=%zu\n", d.vertices.size());
        printf("obj.usemtl=%s\n", d.usemtls.empty() ? "" : d.usemtls[0].name.c_str());
        printf("obj.object=%s\n", d.objects.empty() ? "" : d.objects[0].name.c_str());
        printf("obj.texcoord=%g,%g\n", d.textureCoordinates.size() >= 2 ? d.textureCoordinates[0] : -1.f,
               d.textureCoordinates.size() >= 2 ? d.textureCoordinates[1] : -1.f);
    }
    // MTL
    {
        std::string p = write_tmp("newmtl red\nNs 10\nKd 0.25 0.5 0.75\nKs 1 0 0\nd 0.5\n");
        ObjParser::MtlData m;
        bool ok = ObjParser::mtlparse(p.c_str(), m);
        unlink(p.c_str());
        printf("mtl.ok=%d\n", ok);
        printf("mtl.count=%zu\n", m.new_mtl_unmap.size());
        auto it = m.new_mtl_unmap.find("red");
        if (it != m.new_mtl_unmap.end()) {
            printf("mtl.red.Kd=%g,%g,%g\n", it->second->Kd[0], it->second->Kd[1], it->second->Kd[2]);
            printf("mtl.red.Ns=%g\n", it->second->Ns);
            printf("mtl.red.d=%g\n", it->second->d);
        } else
            printf("mtl.red=missing\n");
    }
    // STL (admesh)
    {
        std::string p = write_tmp(unit_cube_stl());
        stl_file stl;
        bool ok = stl_open(&stl, p.c_str());
        unlink(p.c_str());
        printf("stl.ok=%d\n", ok);
        if (ok) {
            printf("stl.facets=%u\n", stl.stats.number_of_facets);
            stl_check_facets_exact(&stl);
            printf("stl.connected_3_edge=%d\n", stl.stats.connected_facets_3_edge);
            stl_fix_normal_directions(&stl);
            stl_fix_normal_values(&stl);
            stl_calculate_volume(&stl);
            printf("stl.volume=%.4f\n", double(stl.stats.volume));
            printf("stl.size=%.2f,%.2f,%.2f\n", double(stl.stats.size(0)), double(stl.stats.size(1)), double(stl.stats.size(2)));
            indexed_triangle_set its;
            stl_generate_shared_vertices(&stl, its);
            printf("stl.shared_vertices=%zu\n", its.vertices.size());
            printf("stl.shared_faces=%zu\n", its.indices.size());
        }
    }
    // G-code reader
    {
        Slic3r::GCodeReader reader;
        std::string out;
        char buf[256];
        reader.parse_buffer("G1 X10 Y20 E1.5 F3000 ; move\nG1 X13 Y24 E2.5\nM104 S215\nG92 E0\nG1 Z0.2 F600\nG1 X0 Y0\n",
            [&](Slic3r::GCodeReader &r, const Slic3r::GCodeReader::GCodeLine &line) {
                float s = 0;
                bool hs = line.has_value('S', s);
                snprintf(buf, sizeof buf, "%s|x=%d:%g|y=%d:%g|e=%d:%g|f=%d:%g|z=%d:%g|S=%d:%g|ext=%d|trav=%d|dxy=%.3f|c=%s\n",
                         std::string(line.cmd()).c_str(), line.has_x(), line.has_x() ? line.x() : 0.f, line.has_y(), line.has_y() ? line.y() : 0.f,
                         line.has_e(), line.has_e() ? line.e() : 0.f, line.has_f(), line.has_f() ? line.f() : 0.f,
                         line.has_z(), line.has_z() ? line.z() : 0.f, hs, s, line.extruding(r), line.travel(),
                         double(line.dist_XY(r)), std::string(line.comment()).c_str());
                out += buf;
            });
        std::istringstream lines(out);
        std::string l;
        int i = 0;
        while (std::getline(lines, l))
            printf("gcode.line%d=%s\n", i++, l.c_str());
        printf("gcode.final_xyz=%g,%g,%g\n", double(reader.x()), double(reader.y()), double(reader.z()));
    }
    // G-code file reader
    {
        std::string p = write_tmp("G1 X1 Y1\nG1 X2 Y2 E1\n;comment only\nG1 X3\n");
        Slic3r::GCodeReader reader;
        std::vector<size_t> ends;
        int n = 0, ext = 0;
        reader.parse_file(p, [&](Slic3r::GCodeReader &r, const Slic3r::GCodeReader::GCodeLine &line) { ++n; ext += line.extruding(r); }, ends);
        unlink(p.c_str());
        printf("gcodefile.lines=%d\n", n);
        printf("gcodefile.extruding=%d\n", ext);
    }
    return 0;
}
