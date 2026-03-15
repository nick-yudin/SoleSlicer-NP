#include "Exception.hpp"
#include "ExtrusionEntity.hpp"
#include "ExtrusionEntityCollection.hpp"
#include "Layer.hpp"
#include "Line.hpp"
#include "SLA/IndexedMesh.hpp"
#include "libslic3r.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace Slic3r {

static void contour_extrusion_entity(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionEntity *extr);

static double follow_slope_down(double angle_rad, double dist)
{
    return -dist * std::sin(angle_rad);
}

static double slope_from_normal(const Eigen::Vector3d &normal)
{
    Eigen::Vector3d n = normal.normalized();
    return std::acos(std::abs(n.z()));
}

static bool contour_extrusion_path(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionPath &path)
{
    if (region->region().config().zaa_region_disable)
        return false;

    if (path.role() != erTopSolidInfill && path.role() != erIroning &&
        path.role() != erExternalPerimeter && path.role() != erPerimeter)
        return false;

    Layer *layer = region->layer();
    const coordf_t mesh_z = layer->print_z + mesh.ground_level();
    const coordf_t min_z = layer->object()->config().zaa_min_z;
    const Points &points = path.polyline.points;
    if (points.size() < 2)
        return false;

    const double resolution_mm = 0.1;
    const coordf_t height = layer->height;
    const double minimize_perimeter_height_angle = region->region().config().zaa_minimize_perimeter_height;

    Points contoured_points;
    std::vector<coordf_t> z_offsets;
    bool was_contoured = false;

    contoured_points.reserve(points.size() * 2);
    z_offsets.reserve(points.size() * 2);

    for (Points::const_iterator it = points.begin(); it != points.end() - 1; ++it) {
        Vec2d p1d(unscale_(it->x()), unscale_(it->y()));
        Vec2d p2d(unscale_((it + 1)->x()), unscale_((it + 1)->y()));
        Linef line(p1d, p2d);

        const double length_mm = line.length();
        const int num_segments = std::max(1, int(std::ceil(length_mm / resolution_mm)));
        Vec2d delta = line.vector();

        for (int i = 0; i <= num_segments; ++i) {
            Vec2d p = p1d + delta * (double(i) / double(num_segments));

            const coordf_t x = p.x();
            const coordf_t y = p.y();

            sla::IndexedMesh::hit_result hit_up = mesh.query_ray_hit({x, y, mesh_z}, {0.0, 0.0, 1.0});
            sla::IndexedMesh::hit_result hit_down = mesh.query_ray_hit({x, y, mesh_z}, {0.0, 0.0, -1.0});

            const bool has_up = hit_up.is_hit();
            const bool has_down = hit_down.is_hit();
            double d = 0.0;
            Vec3d normal(0.0, 0.0, 1.0);
            const bool has_hit = has_up || has_down;
            if (has_hit) {
                const double up = has_up ? hit_up.distance() : std::numeric_limits<double>::infinity();
                const double down = has_down ? hit_down.distance() : std::numeric_limits<double>::infinity();
                const bool use_up = up < down;
                d = use_up ? up : -down;
                normal = use_up ? hit_up.normal() : hit_down.normal();
            }

            double max_up = min_z;
            double min_down = -(height - min_z);
            const double half_width = path.width / 2.0;
            if (path.role() == erIroning) {
                max_up = height;
                min_down = -(height + 0.1);
            }

            if (has_hit) {
                const double slope_rad = slope_from_normal(normal);
                const double slope_degrees = slope_rad * 180.0 / M_PI;

                if (d > min_down && minimize_perimeter_height_angle > 0 &&
                    minimize_perimeter_height_angle < slope_degrees && path.role() == erExternalPerimeter) {
                    double adjustment = follow_slope_down(slope_rad, half_width);
                    d += adjustment;
                    if (d < min_down)
                        d = min_down;
                }
            }

            if (d > max_up + 0.03 || d < min_down) {
                d = 0;
            } else if (d > max_up) {
                d = max_up;
            }

            // Do not raise external walls to avoid visual seam artifacts.
            if (path.role() == erExternalPerimeter && d > 0)
                d = 0;

            if (std::abs(d) > EPSILON)
                was_contoured = true;

            Point new_point = Point::new_scale(p.x(), p.y());
            if (!contoured_points.empty()) {
                const bool same_xy = contoured_points.back() == new_point;
                const bool same_z = std::abs(z_offsets.back() - coordf_t(d)) <= EPSILON;
                if (same_xy && same_z)
                    continue;
            }

            if (contoured_points.size() > 2) {
                const Point &pp0 = contoured_points[contoured_points.size() - 2];
                const Point &pp1 = contoured_points.back();
                const coordf_t z0 = z_offsets[z_offsets.size() - 2];
                const coordf_t z1 = z_offsets.back();
                Vec3d a(unscale_(pp0.x()), unscale_(pp0.y()), z0);
                Vec3d b(unscale_(pp1.x()), unscale_(pp1.y()), z1);
                Vec3d c(p.x(), p.y(), d);
                if ((b - a).squaredNorm() > EPSILON &&
                    line_alg::distance_to_infinite_squared(Linef3(a, b), c) < EPSILON) {
                    contoured_points.back() = new_point;
                    z_offsets.back() = coordf_t(d);
                    continue;
                }
            }

            contoured_points.push_back(new_point);
            z_offsets.push_back(coordf_t(d));
        }
    }

    if (!was_contoured || contoured_points.size() < 2)
        return false;

    path.polyline = Polyline(std::move(contoured_points));
    path.z_offsets = std::move(z_offsets);
    path.z_contoured = true;
    return true;
}

static void contour_extrusion_multipath(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionMultiPath &multipath)
{
    for (ExtrusionPath &path : multipath.paths)
        contour_extrusion_path(region, mesh, path);
}

static void contour_extrusion_loop(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionLoop &loop)
{
    for (ExtrusionPath &path : loop.paths)
        contour_extrusion_path(region, mesh, path);
}

static void contour_extrusion_entity_collection(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionEntityCollection &collection)
{
    for (ExtrusionEntity *entity : collection.entities)
        contour_extrusion_entity(region, mesh, entity);
}

static void contour_extrusion_entity(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionEntity *extr)
{
    if (dynamic_cast<const ExtrusionPathSloped*>(extr) != nullptr)
        return;

    if (ExtrusionMultiPath *multipath = dynamic_cast<ExtrusionMultiPath*>(extr)) {
        contour_extrusion_multipath(region, mesh, *multipath);
        return;
    }

    if (ExtrusionPath *path = dynamic_cast<ExtrusionPath*>(extr)) {
        contour_extrusion_path(region, mesh, *path);
        return;
    }

    if (ExtrusionLoop *loop = dynamic_cast<ExtrusionLoop*>(extr)) {
        contour_extrusion_loop(region, mesh, *loop);
        return;
    }

    if (dynamic_cast<const ExtrusionLoopSloped*>(extr) != nullptr)
        return;

    if (ExtrusionEntityCollection *collection = dynamic_cast<ExtrusionEntityCollection*>(extr)) {
        contour_extrusion_entity_collection(region, mesh, *collection);
        return;
    }
}

static bool role_allowed(const ExtrusionRole role, const std::initializer_list<ExtrusionRole> &roles)
{
    for (ExtrusionRole allowed : roles)
        if (role == allowed)
            return true;
    return false;
}

static void handle_extrusion_collection(LayerRegion *region, const sla::IndexedMesh &mesh, ExtrusionEntityCollection &collection, std::initializer_list<ExtrusionRole> roles)
{
    for (ExtrusionEntity *extr : collection.entities) {
        if (!role_allowed(extr->role(), roles))
            continue;
        contour_extrusion_entity(region, mesh, extr);
    }
}

void Layer::make_contour_z(const sla::IndexedMesh &mesh)
{
    for (LayerRegion *region : this->regions()) {
        handle_extrusion_collection(region, mesh, region->fills, {erTopSolidInfill, erIroning, erExternalPerimeter, erMixed});
        handle_extrusion_collection(region, mesh, region->perimeters, {erExternalPerimeter, erMixed});
    }
}

} // namespace Slic3r
