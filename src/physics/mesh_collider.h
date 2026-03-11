#pragma once
#include "physics/collision.h"
#include <vector>

namespace odyssey::physics {

struct Triangle {
    vec3 v0, v1, v2;
    vec3 normal;
};

class MeshCollider {
public:
    void build(std::vector<Triangle> triangles);
    void build_from_mesh(const vec3* verts, uint32_t vert_count,
                         const uint32_t* indices, uint32_t idx_count);
    bool raycast(const Ray& ray, RayHit& hit, float max_dist = 1000.0f) const;
    bool height_at(float x, float z, float& out_height) const;

    bool empty() const { return triangles_.empty(); }

private:
    std::vector<Triangle> triangles_;

    // Flat grid spatial acceleration
    struct GridCell {
        std::vector<uint32_t> triangle_indices;
    };

    std::vector<GridCell> grid_;
    float grid_min_x_ = 0, grid_min_z_ = 0;
    float grid_cell_size_ = 1.0f;
    uint32_t grid_width_ = 0, grid_depth_ = 0;

    void build_grid();
    const GridCell* cell_at(float x, float z) const;

    // Moller-Trumbore ray-triangle intersection
    static bool ray_triangle(const Ray& ray, const Triangle& tri,
                             float& t, float max_dist);
};

} // namespace odyssey::physics
