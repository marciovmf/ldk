#include <ldk_raycast.h>
#include <float.h>
#include <math.h>

#define LDK_RAYCAST_EPSILON 0.000001f

bool ldk_ray_make(Vec3 origin, Vec3 direction, LDKRay *out_ray)
{
  float direction_length_squared;

  if (out_ray == NULL)
  {
    return false;
  }

  direction_length_squared = vec3_len2(direction);
  if (direction_length_squared <=
      LDK_RAYCAST_EPSILON * LDK_RAYCAST_EPSILON)
  {
    return false;
  }

  out_ray->origin = origin;
  out_ray->direction = vec3_norm(direction);
  return true;
}

Vec3 ldk_ray_at(LDKRay ray, float distance)
{
  return vec3_add(ray.origin, vec3_mul(ray.direction, distance));
}

bool ldk_ray_transform(LDKRay ray, Mat4 transform, LDKRay *out_ray)
{
  Vec3 origin;
  Vec3 direction;

  if (out_ray == NULL)
  {
    return false;
  }

  origin = mat4_mul_point(transform, ray.origin);
  direction = mat4_mul_dir(transform, ray.direction);
  return ldk_ray_make(origin, direction, out_ray);
}

bool ldk_raycast_plane(LDKRay ray, Vec3 plane_point,
    Vec3 plane_normal, LDKRaycastHit *out_hit)
{
  Vec3 normal;
  float denominator;
  float distance;

  if (vec3_len2(plane_normal) <=
      LDK_RAYCAST_EPSILON * LDK_RAYCAST_EPSILON)
  {
    return false;
  }

  normal = vec3_norm(plane_normal);
  denominator = vec3_dot(ray.direction, normal);

  if (fabsf(denominator) <= LDK_RAYCAST_EPSILON)
  {
    return false;
  }

  distance =
      vec3_dot(vec3_sub(plane_point, ray.origin), normal) / denominator;

  if (distance < 0.0f)
  {
    return false;
  }

  if (out_hit != NULL)
  {
    out_hit->position = ldk_ray_at(ray, distance);
    out_hit->normal = normal;
    out_hit->distance = distance;
    out_hit->triangle_index = UINT32_MAX;
  }

  return true;
}

bool ldk_raycast_triangle(LDKRay ray, Vec3 a, Vec3 b, Vec3 c,
    LDKRaycastHit *out_hit)
{
  Vec3 edge_ab = vec3_sub(b, a);
  Vec3 edge_ac = vec3_sub(c, a);
  Vec3 p = vec3_cross(ray.direction, edge_ac);
  float determinant = vec3_dot(edge_ab, p);

  if (fabsf(determinant) <= LDK_RAYCAST_EPSILON)
  {
    return false;
  }

  float inverse_determinant = 1.0f / determinant;
  Vec3 origin_to_a = vec3_sub(ray.origin, a);
  float u = vec3_dot(origin_to_a, p) * inverse_determinant;

  if (u < 0.0f || u > 1.0f)
  {
    return false;
  }

  Vec3 q = vec3_cross(origin_to_a, edge_ab);
  float v = vec3_dot(ray.direction, q) * inverse_determinant;

  if (v < 0.0f || u + v > 1.0f)
  {
    return false;
  }

  float distance = vec3_dot(edge_ac, q) * inverse_determinant;
  if (distance < 0.0f)
  {
    return false;
  }

  if (out_hit != NULL)
  {
    Vec3 normal = vec3_cross(edge_ab, edge_ac);

    out_hit->position = ldk_ray_at(ray, distance);
    out_hit->normal = vec3_norm(normal);
    out_hit->distance = distance;
    out_hit->triangle_index = 0;
  }

  return true;
}

bool ldk_raycast_mesh(
    LDKRay ray, const LDKMeshData *mesh, LDKRaycastHit *out_hit)
{
  bool has_hit = false;
  LDKRaycastHit nearest_hit = {0};
  float nearest_distance = FLT_MAX;

  if (mesh == NULL ||
      mesh->vertices == NULL ||
      mesh->indices == NULL ||
      mesh->vertex_count == 0 ||
      mesh->index_count < 3)
  {
    return false;
  }

  for (u32 index = 0; index + 2 < mesh->index_count; index += 3)
  {
    u32 ia = mesh->indices[index];
    u32 ib = mesh->indices[index + 1];
    u32 ic = mesh->indices[index + 2];
    LDKRaycastHit hit;

    if (ia >= mesh->vertex_count ||
        ib >= mesh->vertex_count ||
        ic >= mesh->vertex_count)
    {
      continue;
    }

    if (!ldk_raycast_triangle(ray,
            mesh->vertices[ia].position,
            mesh->vertices[ib].position,
            mesh->vertices[ic].position,
            &hit))
    {
      continue;
    }

    if (hit.distance < nearest_distance)
    {
      nearest_distance = hit.distance;
      nearest_hit = hit;
      nearest_hit.triangle_index = index / 3u;
      has_hit = true;
    }
  }

  if (has_hit && out_hit != NULL)
  {
    *out_hit = nearest_hit;
  }

  return has_hit;
}

bool ldk_raycast_mesh_transformed(LDKRay ray,
    const LDKMeshData *mesh, Mat4 mesh_world, LDKRaycastHit *out_hit)
{
  bool has_hit = false;
  LDKRaycastHit nearest_hit = {0};
  float nearest_distance = FLT_MAX;

  if (mesh == NULL ||
      mesh->vertices == NULL ||
      mesh->indices == NULL ||
      mesh->vertex_count == 0 ||
      mesh->index_count < 3)
  {
    return false;
  }

  for (u32 index = 0; index + 2 < mesh->index_count; index += 3)
  {
    u32 ia = mesh->indices[index];
    u32 ib = mesh->indices[index + 1];
    u32 ic = mesh->indices[index + 2];
    LDKRaycastHit hit;

    if (ia >= mesh->vertex_count ||
        ib >= mesh->vertex_count ||
        ic >= mesh->vertex_count)
    {
      continue;
    }

    Vec3 a = mat4_mul_point(mesh_world, mesh->vertices[ia].position);
    Vec3 b = mat4_mul_point(mesh_world, mesh->vertices[ib].position);
    Vec3 c = mat4_mul_point(mesh_world, mesh->vertices[ic].position);

    if (!ldk_raycast_triangle(ray, a, b, c, &hit))
    {
      continue;
    }

    if (hit.distance < nearest_distance)
    {
      nearest_distance = hit.distance;
      nearest_hit = hit;
      nearest_hit.triangle_index = index / 3u;
      has_hit = true;
    }
  }

  if (has_hit && out_hit != NULL)
  {
    *out_hit = nearest_hit;
  }

  return has_hit;
}
