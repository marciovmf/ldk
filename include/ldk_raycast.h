#ifndef LDK_RAYCAST_H
#define LDK_RAYCAST_H

#include <ldk_common.h>
#include <ldk_mesh.h>
#include <stdx/stdx_math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LDKRay
{
  Vec3 origin;
  Vec3 direction;
} LDKRay;

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
LDK_API bool ldk_ray_make(Vec3 origin, Vec3 direction, LDKRay *out_ray);

/* Returns origin + direction * distance. */
LDK_API Vec3 ldk_ray_at(LDKRay ray, float distance);

/*
 * Transforms a ray.
 *
 * The origin is transformed as a point and the direction as a direction.
 * The resulting direction is normalized.
 */
LDK_API bool ldk_ray_transform(
    LDKRay ray, Mat4 transform, LDKRay *out_ray);

/* Double-sided ray/plane intersection. */
LDK_API bool ldk_raycast_plane(LDKRay ray, Vec3 plane_point,
    Vec3 plane_normal, LDKRaycastHit *out_hit);

/* Double-sided Moller-Trumbore ray/triangle intersection. */
LDK_API bool ldk_raycast_triangle(LDKRay ray, Vec3 a, Vec3 b, Vec3 c,
    LDKRaycastHit *out_hit);

/*
 * Tests an indexed triangle mesh in its current coordinate space.
 * triangle_index is the zero-based triangle number.
 */
LDK_API bool ldk_raycast_mesh(
    LDKRay ray, const LDKMeshData *mesh, LDKRaycastHit *out_hit);

/*
 * Tests an indexed mesh after transforming its triangles to world space.
 *
 * This avoids transforming the ray through an inverse world matrix and is
 * intentionally simple for editor picking, which only happens on click.
 */
LDK_API bool ldk_raycast_mesh_transformed(LDKRay ray,
    const LDKMeshData *mesh, Mat4 mesh_world, LDKRaycastHit *out_hit);

#ifdef __cplusplus
}
#endif

#endif // LDK_RAYCAST_H
