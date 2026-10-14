/**
 * # Ray intersections
 *
 * Raycasting and geometric intersection queries.
 */

#ifndef LDK_RAYCAST_H
#define LDK_RAYCAST_H

#include <ldk_common.h>
#include <ldk_mesh.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @group ldk.raycast */
typedef struct LDKRay
{
  Vec3 origin;
  Vec3 direction;
} LDKRay;

/** @group ldk.raycast */
typedef struct LDKRaycastHit
{
  Vec3 position;
  Vec3 normal;
  float distance;
  u32 triangle_index;
} LDKRaycastHit;

/*
 * Creates a ray with a normalized direction.
 * Returns false when direction is too small to normalize.
 */
/**
 * @brief Construct a 3D ray from origin and direction.
 * @param origin Origin used by this operation.
 * @param direction Direction used by this operation.
 * @param out_ray Receives the resulting ray.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_ray_make(Vec3 origin, Vec3 direction, LDKRay *out_ray);

/* Returns origin + direction * distance. */
/**
 * @brief Compute the 3D position along a ray at the specified distance.
 * @param ray Ray used by this operation.
 * @param distance Distance in world-space units.
 * @return The requested value.
 * @group ldk.raycast
 */
LDK_API Vec3 ldk_ray_at(LDKRay ray, float distance);

/*
 * Transforms a ray.
 *
 * The origin is transformed as a point and the direction as a direction.
 * The resulting direction is normalized.
 */
/**
 * @brief Transform a ray using a matrix.
 * @param ray Ray used by this operation.
 * @param transform Transform used by this operation.
 * @param out_ray Receives the resulting ray.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_ray_transform(
    LDKRay ray, Mat4 transform, LDKRay *out_ray);

/* Double-sided ray/plane intersection. */
/**
 * @brief Test the intersection of a ray with a plane.
 * @param ray Ray used by this operation.
 * @param plane_point Plane point used by this operation.
 * @param plane_normal Plane normal used by this operation.
 * @param out_hit Receives the intersection details.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_raycast_plane(LDKRay ray, Vec3 plane_point,
    Vec3 plane_normal, LDKRaycastHit *out_hit);

/* Double-sided Moller-Trumbore ray/triangle intersection. */
/**
 * @brief Test the intersection of a ray with a triangle.
 * @param ray Ray used by this operation.
 * @param a First value to compare or combine.
 * @param b Second value to compare or combine.
 * @param c C used by this operation.
 * @param out_hit Receives the intersection details.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_raycast_triangle(LDKRay ray, Vec3 a, Vec3 b, Vec3 c,
    LDKRaycastHit *out_hit);

/*
 * Tests an indexed triangle mesh in its current coordinate space.
 * triangle_index is the zero-based triangle number.
 */
/**
 * @brief Intersect a ray against mesh geometry.
 * @param ray Ray used by this operation.
 * @param mesh Mesh resource used by the operation.
 * @param out_hit Receives the intersection details.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_raycast_mesh(
    LDKRay ray, const LDKMeshData *mesh, LDKRaycastHit *out_hit);

/*
 * Tests an indexed mesh after transforming its triangles to world space.
 *
 * This avoids transforming the ray through an inverse world matrix and is
 * intentionally simple for editor picking, which only happens on click.
 */
/**
 * @brief Intersect a ray against transformed mesh geometry.
 * @param ray Ray used by this operation.
 * @param mesh Mesh resource used by the operation.
 * @param mesh_world Mesh world used by this operation.
 * @param out_hit Receives the intersection details.
 * @return True if the operation succeeds; false otherwise.
 * @group ldk.raycast
 */
LDK_API bool ldk_raycast_mesh_transformed(LDKRay ray,
    const LDKMeshData *mesh, Mat4 mesh_world, LDKRaycastHit *out_hit);

#ifdef __cplusplus
}
#endif

#endif // LDK_RAYCAST_H
