#include "physics/mesh_collider.h"
#include <glm/geometric.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace odyssey::physics {

void MeshCollider::build(std::vector<Triangle> triangles) {
    triangles_ = std::move(triangles);
    build_grid();
}

void MeshCollider::build_from_mesh(const vec3* verts, uint32_t vert_count,
                                   const uint32_t* indices, uint32_t idx_count) {
    (void)vert_count; // used implicitly via indices

    std::vector<Triangle> tris;
    tris.reserve(idx_count / 3);

    for (uint32_t i = 0; i + 2 < idx_count; i += 3) {
        Triangle tri;
        tri.v0 = verts[indices[i]];
        tri.v1 = verts[indices[i + 1]];
        tri.v2 = verts[indices[i + 2]];

        vec3 edge1 = tri.v1 - tri.v0;
        vec3 edge2 = tri.v2 - tri.v0;
        vec3 n = glm::cross(edge1, edge2);
        float len = glm::length(n);
        tri.normal = (len > 1e-8f) ? (n / len) : vec3(0.0f, 1.0f, 0.0f);

        tris.push_back(tri);
    }

    build(std::move(tris));
}

void MeshCollider::build_grid() {
    if (triangles_.empty()) {
        grid_.clear();
        grid_width_ = 0;
        grid_depth_ = 0;
        return;
    }

    // Compute AABB of all triangle vertices
    float min_x = std::numeric_limits<float>::max();
    float min_z = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float max_z = std::numeric_limits<float>::lowest();

    for (const auto& tri : triangles_) {
        for (const vec3& v : {tri.v0, tri.v1, tri.v2}) {
            min_x = std::min(min_x, v.x);
            min_z = std::min(min_z, v.z);
            max_x = std::max(max_x, v.x);
            max_z = std::max(max_z, v.z);
        }
    }

    // Add small padding
    min_x -= 0.1f;
    min_z -= 0.1f;
    max_x += 0.1f;
    max_z += 0.1f;

    grid_min_x_ = min_x;
    grid_min_z_ = min_z;
    grid_cell_size_ = 2.0f;

    grid_width_ = static_cast<uint32_t>(std::ceil((max_x - min_x) / grid_cell_size_));
    grid_depth_ = static_cast<uint32_t>(std::ceil((max_z - min_z) / grid_cell_size_));

    // Ensure at least 1x1
    grid_width_ = std::max(grid_width_, 1u);
    grid_depth_ = std::max(grid_depth_, 1u);

    grid_.clear();
    grid_.resize(static_cast<size_t>(grid_width_) * grid_depth_);

    // Assign each triangle to all cells it overlaps
    for (uint32_t ti = 0; ti < static_cast<uint32_t>(triangles_.size()); ++ti) {
        const auto& tri = triangles_[ti];

        // Compute triangle AABB in XZ
        float tri_min_x = std::min({tri.v0.x, tri.v1.x, tri.v2.x});
        float tri_max_x = std::max({tri.v0.x, tri.v1.x, tri.v2.x});
        float tri_min_z = std::min({tri.v0.z, tri.v1.z, tri.v2.z});
        float tri_max_z = std::max({tri.v0.z, tri.v1.z, tri.v2.z});

        int cx0 = std::max(0, static_cast<int>((tri_min_x - grid_min_x_) / grid_cell_size_));
        int cx1 = std::min(static_cast<int>(grid_width_) - 1,
                           static_cast<int>((tri_max_x - grid_min_x_) / grid_cell_size_));
        int cz0 = std::max(0, static_cast<int>((tri_min_z - grid_min_z_) / grid_cell_size_));
        int cz1 = std::min(static_cast<int>(grid_depth_) - 1,
                           static_cast<int>((tri_max_z - grid_min_z_) / grid_cell_size_));

        for (int cz = cz0; cz <= cz1; ++cz) {
            for (int cx = cx0; cx <= cx1; ++cx) {
                grid_[static_cast<size_t>(cz) * grid_width_ + cx].triangle_indices.push_back(ti);
            }
        }
    }
}

const MeshCollider::GridCell* MeshCollider::cell_at(float x, float z) const {
    if (grid_.empty()) return nullptr;

    int cx = static_cast<int>((x - grid_min_x_) / grid_cell_size_);
    int cz = static_cast<int>((z - grid_min_z_) / grid_cell_size_);

    if (cx < 0 || cx >= static_cast<int>(grid_width_) ||
        cz < 0 || cz >= static_cast<int>(grid_depth_)) {
        return nullptr;
    }

    return &grid_[static_cast<size_t>(cz) * grid_width_ + cx];
}

bool MeshCollider::ray_triangle(const Ray& ray, const Triangle& tri,
                                float& t, float max_dist) {
    constexpr float EPSILON = 1e-7f;

    vec3 edge1 = tri.v1 - tri.v0;
    vec3 edge2 = tri.v2 - tri.v0;

    vec3 p = glm::cross(ray.direction, edge2);
    float det = glm::dot(edge1, p);

    // If determinant is near zero, the ray lies in the plane of the triangle
    if (std::abs(det) < EPSILON) {
        return false;
    }

    float inv_det = 1.0f / det;

    vec3 tvec = ray.origin - tri.v0;
    float u = glm::dot(tvec, p) * inv_det;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    vec3 q = glm::cross(tvec, edge1);
    float v = glm::dot(ray.direction, q) * inv_det;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t_hit = glm::dot(edge2, q) * inv_det;
    if (t_hit < 0.0f || t_hit > max_dist) {
        return false;
    }

    t = t_hit;
    return true;
}

bool MeshCollider::raycast(const Ray& ray, RayHit& hit, float max_dist) const {
    if (triangles_.empty()) return false;

    float closest_t = max_dist;
    uint32_t closest_tri = UINT32_MAX;

    // Brute-force all triangles (grid acceleration is used for height_at)
    for (uint32_t i = 0; i < static_cast<uint32_t>(triangles_.size()); ++i) {
        float t = 0.0f;
        if (ray_triangle(ray, triangles_[i], t, closest_t)) {
            if (t < closest_t) {
                closest_t = t;
                closest_tri = i;
            }
        }
    }

    if (closest_tri == UINT32_MAX) {
        return false;
    }

    hit.point = ray.origin + ray.direction * closest_t;
    hit.normal = triangles_[closest_tri].normal;
    hit.distance = closest_t;
    hit.triangle_index = closest_tri;
    return true;
}

bool MeshCollider::height_at(float x, float z, float& out_height) const {
    const GridCell* cell = cell_at(x, z);
    if (!cell || cell->triangle_indices.empty()) {
        return false;
    }

    // Cast a downward ray from high above
    Ray ray;
    ray.origin = vec3(x, 10000.0f, z);
    ray.direction = vec3(0.0f, -1.0f, 0.0f);

    float best_height = std::numeric_limits<float>::lowest();
    bool found = false;

    for (uint32_t ti : cell->triangle_indices) {
        float t = 0.0f;
        if (ray_triangle(ray, triangles_[ti], t, 20000.0f)) {
            float hit_y = ray.origin.y + ray.direction.y * t; // = 10000 - t
            if (hit_y > best_height) {
                best_height = hit_y;
                found = true;
            }
        }
    }

    if (found) {
        out_height = best_height;
    }
    return found;
}

} // namespace odyssey::physics
