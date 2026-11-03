// Fuzzes the STL loader + the import-time mesh repair pipeline (src/admesh, mirroring
// TriangleMesh::ReadSTLFile -> trianglemesh_repair_on_import in src/libslic3r/TriangleMesh.cpp).
#include "quiet_log.h"
#include <cstddef>
#include <cstdint>

#include <admesh/stl.h>
#include "scratch_file.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ScratchFile f(data, size);
    if (!f.ok)
        return 0;
    stl_file stl;
    if (!stl_open(&stl, f.path.c_str()))
        return 0;

    // trianglemesh_repair_on_import returns early on an empty mesh ("admesh fails when repairing
    // empty meshes"); TriangleMesh::from_stl still builds the shared-vertex set afterwards.
    if (stl.stats.number_of_facets == 0) {
        indexed_triangle_set empty_its;
        stl_generate_shared_vertices(&stl, empty_its);
        return 0;
    }

    stl_check_facets_exact(&stl);
    stl.stats.facets_w_1_bad_edge = (stl.stats.connected_facets_2_edge - stl.stats.connected_facets_3_edge);
    stl.stats.facets_w_2_bad_edge = (stl.stats.connected_facets_1_edge - stl.stats.connected_facets_2_edge);
    stl.stats.facets_w_3_bad_edge = (stl.stats.number_of_facets - stl.stats.connected_facets_1_edge);

    float tolerance = (float) stl.stats.shortest_edge;
    float increment = (float) stl.stats.bounding_diameter / 10000.0f;
    for (int i = 0; i < 2 && stl.stats.connected_facets_3_edge < int(stl.stats.number_of_facets); ++i) {
        stl_check_facets_nearby(&stl, tolerance);
        tolerance += increment;
    }
    if (stl.stats.connected_facets_3_edge < (int) stl.stats.number_of_facets)
        stl_remove_unconnected_facets(&stl);
    stl_fix_normal_directions(&stl);
    stl_fix_normal_values(&stl);
    stl_calculate_volume(&stl);
    stl_verify_neighbors(&stl);
    if (stl.stats.number_of_facets > 0 && stl.stats.degenerate_facets > 0)
        stl_check_facets_exact(&stl);

    indexed_triangle_set its;
    stl_generate_shared_vertices(&stl, its);
    return 0;
}
