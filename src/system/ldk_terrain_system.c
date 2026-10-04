#include <system/ldk_terrain_system.h>

#include <ldk.h>
#include <ldk_image.h>
#include <ldk_material_asset.h>
#include <ldk_mesh.h>
#include <module/ldk_asset_manager.h>
#include <module/ldk_renderer.h>
#include <stdx/stdx_math.h>

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct LDKTerrainChunk
{
  LDKResourceMesh mesh;
  Vec3 bounds_min;
  Vec3 bounds_max;
} LDKTerrainChunk;

typedef struct LDKTerrainRuntime
{
  LDKTerrainSystem *owner;
  LDKTerrainChunk *chunks;
  u32 chunk_capacity;
  LDKImageInfo source;
  LDKAssetImage source_asset;
  u64 source_revision;
  u64 geometry_hash;
  u64 appearance_hash;

  LDKAssetMaterial material_asset;
  u64 material_revision;
  LDKResourceMaterial material;
  LDKResourceTexture texture;
  LDKResourceTexture normal_map;
  LDKResourceTexture specular_map;
} LDKTerrainRuntime;

static LDKTerrainRuntime s_runtime;
static LDKAssetImage s_pending_heightmap;
static bool s_pending_heightmap_valid;

static u64 s_hash_bytes(u64 hash, const void *data, size_t size)
{
  const u8 *bytes = (const u8 *)data;
  for (size_t i = 0u; i < size; ++i)
  {
    hash ^= (u64)bytes[i];
    hash *= UINT64_C(1099511628211);
  }
  return hash;
}

static bool s_asset_image_equal(LDKAssetImage a, LDKAssetImage b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static bool s_asset_material_equal(LDKAssetMaterial a, LDKAssetMaterial b)
{
  return a.h.index == b.h.index && a.h.version == b.h.version;
}

static float s_surface_min_height(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_min_height;
  case 1: return s->surface_1_min_height;
  case 2: return s->surface_2_min_height;
  case 3: return s->surface_3_min_height;
  case 4: return s->surface_4_min_height;
  case 5: return s->surface_5_min_height;
  case 6: return s->surface_6_min_height;
  case 7: return s->surface_7_min_height;
  default: return 0.0f;
  }
}

static float s_surface_blend_width(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_blend_width;
  case 1: return s->surface_1_blend_width;
  case 2: return s->surface_2_blend_width;
  case 3: return s->surface_3_blend_width;
  case 4: return s->surface_4_blend_width;
  case 5: return s->surface_5_blend_width;
  case 6: return s->surface_6_blend_width;
  case 7: return s->surface_7_blend_width;
  default: return 0.0f;
  }
}

static rgba32 s_surface_color(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_color;
  case 1: return s->surface_1_color;
  case 2: return s->surface_2_color;
  case 3: return s->surface_3_color;
  case 4: return s->surface_4_color;
  case 5: return s->surface_5_color;
  case 6: return s->surface_6_color;
  case 7: return s->surface_7_color;
  default: return LDK_RGBA32(0xffffffffu);
  }
}

static Vec4 s_surface_atlas_rect(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_atlas_rect;
  case 1: return s->surface_1_atlas_rect;
  case 2: return s->surface_2_atlas_rect;
  case 3: return s->surface_3_atlas_rect;
  case 4: return s->surface_4_atlas_rect;
  case 5: return s->surface_5_atlas_rect;
  case 6: return s->surface_6_atlas_rect;
  case 7: return s->surface_7_atlas_rect;
  default: return vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
  }
}

static Vec2 s_surface_uv_scale(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_uv_scale;
  case 1: return s->surface_1_uv_scale;
  case 2: return s->surface_2_uv_scale;
  case 3: return s->surface_3_uv_scale;
  case 4: return s->surface_4_uv_scale;
  case 5: return s->surface_5_uv_scale;
  case 6: return s->surface_6_uv_scale;
  case 7: return s->surface_7_uv_scale;
  default: return vec2_make(1.0f, 1.0f);
  }
}

static float s_surface_texture_weight(const LDKTerrainSystem *s, u32 i)
{
  switch (i)
  {
  case 0: return s->surface_0_texture_weight;
  case 1: return s->surface_1_texture_weight;
  case 2: return s->surface_2_texture_weight;
  case 3: return s->surface_3_texture_weight;
  case 4: return s->surface_4_texture_weight;
  case 5: return s->surface_5_texture_weight;
  case 6: return s->surface_6_texture_weight;
  case 7: return s->surface_7_texture_weight;
  default: return 0.0f;
  }
}

static void s_surface_defaults(LDKTerrainSystem *system)
{
  for (u32 i = 0u; i < LDK_TERRAIN_SURFACE_COUNT; ++i)
  {
    Vec4 *rect = NULL;
    Vec2 *scale = NULL;
    rgba32 *color = NULL;
    switch (i)
    {
    case 0: rect=&system->surface_0_atlas_rect; scale=&system->surface_0_uv_scale; color=&system->surface_0_color; break;
    case 1: rect=&system->surface_1_atlas_rect; scale=&system->surface_1_uv_scale; color=&system->surface_1_color; break;
    case 2: rect=&system->surface_2_atlas_rect; scale=&system->surface_2_uv_scale; color=&system->surface_2_color; break;
    case 3: rect=&system->surface_3_atlas_rect; scale=&system->surface_3_uv_scale; color=&system->surface_3_color; break;
    case 4: rect=&system->surface_4_atlas_rect; scale=&system->surface_4_uv_scale; color=&system->surface_4_color; break;
    case 5: rect=&system->surface_5_atlas_rect; scale=&system->surface_5_uv_scale; color=&system->surface_5_color; break;
    case 6: rect=&system->surface_6_atlas_rect; scale=&system->surface_6_uv_scale; color=&system->surface_6_color; break;
    case 7: rect=&system->surface_7_atlas_rect; scale=&system->surface_7_uv_scale; color=&system->surface_7_color; break;
    }
    if (rect && rect->x == 0.0f && rect->y == 0.0f &&
        rect->z == 0.0f && rect->w == 0.0f)
      *rect = vec4_make(0.0f, 0.0f, 1.0f, 1.0f);
    if (scale && scale->x == 0.0f && scale->y == 0.0f)
      *scale = vec2_make(1.0f, 1.0f);
    if (color && *color == 0u)
      *color = LDK_RGBA32(0xffffffffu);
  }
}

static bool s_config_is_valid(const LDKTerrainSystem *system)
{
  if (!system || system->height_channel > LDK_TERRAIN_CHANNEL_A ||
      !isfinite(system->cell_size) || system->cell_size <= 0.0f ||
      !isfinite(system->height_scale) || system->height_scale <= 0.0f ||
      !isfinite(system->height_offset) ||
      !isfinite(system->origin_x) || !isfinite(system->origin_z) ||
      system->chunk_size == 0u ||
      system->surface_count == 0u ||
      system->surface_count > LDK_TERRAIN_SURFACE_COUNT)
    return false;

  float previous = -INFINITY;
  for (u32 i = 0u; i < system->surface_count; ++i)
  {
    float h = s_surface_min_height(system, i);
    float blend = s_surface_blend_width(system, i);
    float weight = s_surface_texture_weight(system, i);
    Vec4 rect = s_surface_atlas_rect(system, i);
    Vec2 uv = s_surface_uv_scale(system, i);
    if (!isfinite(h) || h < 0.0f || h > 1.0f || h < previous ||
        !isfinite(blend) || blend < 0.0f || blend > 1.0f ||
        !isfinite(weight) || weight < 0.0f || weight > 1.0f ||
        !isfinite(rect.x) || !isfinite(rect.y) ||
        !isfinite(rect.z) || !isfinite(rect.w) ||
        !isfinite(uv.x) || !isfinite(uv.y))
      return false;
    previous = h;
  }
  return true;
}

static bool s_source_refresh(LDKTerrainSystem *system, LDKAssetManager *assets)
{
  const LDKAssetImageData *data;
  LDKAssetHandle generic;
  const LDKAssetInfo *asset_info;

  if (!system || !assets || x_handle_is_null(system->heightmap.h))
    return false;

  data = ldk_asset_manager_image_get_const(assets, system->heightmap);
  if (!data || !data->image || data->is_missing ||
      !ldk_image_get_info(data->image, &s_runtime.source) ||
      s_runtime.source.channel_count < 4u ||
      (s_runtime.source.format != LDK_IMAGE_FORMAT_RGBA8_UNORM &&
       s_runtime.source.format != LDK_IMAGE_FORMAT_RGBA16_UNORM) ||
      s_runtime.source.width == 0u || s_runtime.source.height == 0u)
    return false;

  generic.h = system->heightmap.h;
  asset_info = ldk_asset_get_info_const(assets, generic);
  s_runtime.source_asset = system->heightmap;
  s_runtime.source_revision = asset_info ? asset_info->source_revision : 0u;
  return true;
}

static float s_texel_channel(u32 x, u32 y, LDKTerrainChannel channel)
{
  const LDKImageInfo *info = &s_runtime.source;
  if (!info->pixels || x >= info->width || y >= info->height ||
      channel > LDK_TERRAIN_CHANNEL_A)
    return 0.0f;

  size_t index = ((size_t)y * info->width + x) * info->channel_count +
      (size_t)channel;
  if (info->format == LDK_IMAGE_FORMAT_RGBA16_UNORM)
    return (float)((const u16 *)info->pixels)[index] * (1.0f / 65535.0f);
  return (float)info->pixels[index] * (1.0f / 255.0f);
}

static float s_vertex_normalized_height(
    const LDKTerrainSystem *system, u32 vx, u32 vy)
{
  float total = 0.0f;
  u32 count = 0u;
  for (i32 oy = -1; oy <= 0; ++oy)
  {
    i64 y = (i64)vy + oy;
    if (y < 0 || y >= (i64)s_runtime.source.height)
      continue;
    for (i32 ox = -1; ox <= 0; ++ox)
    {
      i64 x = (i64)vx + ox;
      if (x < 0 || x >= (i64)s_runtime.source.width)
        continue;
      total += s_texel_channel((u32)x, (u32)y, system->height_channel);
      ++count;
    }
  }
  return count ? total / (float)count : 0.0f;
}

static float s_vertex_world_height(
    const LDKTerrainSystem *system, u32 x, u32 y)
{
  return system->height_offset +
      s_vertex_normalized_height(system, x, y) * system->height_scale;
}

static void s_world_origin(
    const LDKTerrainSystem *system, float *out_x, float *out_z)
{
  float x = system->origin_x;
  float z = system->origin_z;
  if (system->center_on_origin)
  {
    x -= (float)s_runtime.source.width * system->cell_size * 0.5f;
    z -= (float)s_runtime.source.height * system->cell_size * 0.5f;
  }
  *out_x = x;
  *out_z = z;
}

static Vec3 s_vertex_normal(const LDKTerrainSystem *system, u32 x, u32 y)
{
  u32 max_x = s_runtime.source.width;
  u32 max_y = s_runtime.source.height;
  u32 left = x > 0u ? x - 1u : x;
  u32 right = x < max_x ? x + 1u : x;
  u32 down = y > 0u ? y - 1u : y;
  u32 up = y < max_y ? y + 1u : y;
  float h0 = s_vertex_world_height(system, left, y);
  float h1 = s_vertex_world_height(system, right, y);
  float z0 = s_vertex_world_height(system, x, down);
  float z1 = s_vertex_world_height(system, x, up);
  float dx = (float)(right - left) * system->cell_size;
  float dz = (float)(up - down) * system->cell_size;
  Vec3 n = vec3_make(dx > 0.0f ? -(h1-h0)/dx : 0.0f,
      1.0f, dz > 0.0f ? -(z1-z0)/dz : 0.0f);
  float len = sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
  return len > 1e-8f ? vec3_mul(n, 1.0f/len) : vec3_make(0,1,0);
}

static void s_chunks_destroy(LDKRenderer *renderer)
{
  if (renderer)
  {
    for (u32 i = 0u; i < s_runtime.owner->chunk_count; ++i)
    {
      if (ldk_renderer_mesh_is_valid(renderer, s_runtime.chunks[i].mesh))
        ldk_renderer_mesh_destroy(renderer, s_runtime.chunks[i].mesh);
    }
  }
  free(s_runtime.chunks);
  s_runtime.chunks = NULL;
  s_runtime.chunk_capacity = 0u;
  if (s_runtime.owner)
  {
    s_runtime.owner->chunk_count = 0u;
    s_runtime.owner->visible_chunk_count = 0u;
    s_runtime.owner->has_geometry = false;
  }
}

static u64 s_geometry_hash(const LDKTerrainSystem *s)
{
  u64 h = UINT64_C(14695981039346656037);
  h=s_hash_bytes(h,&s->height_channel,sizeof(s->height_channel));
  h=s_hash_bytes(h,&s->cell_size,sizeof(s->cell_size));
  h=s_hash_bytes(h,&s->height_scale,sizeof(s->height_scale));
  h=s_hash_bytes(h,&s->height_offset,sizeof(s->height_offset));
  h=s_hash_bytes(h,&s->center_on_origin,sizeof(s->center_on_origin));
  h=s_hash_bytes(h,&s->origin_x,sizeof(s->origin_x));
  h=s_hash_bytes(h,&s->origin_z,sizeof(s->origin_z));
  h=s_hash_bytes(h,&s->chunk_size,sizeof(s->chunk_size));
  h=s_hash_bytes(h,&s->heightmap,sizeof(s->heightmap));
  h=s_hash_bytes(h,&s_runtime.source_revision,sizeof(s_runtime.source_revision));
  return h;
}

static u64 s_appearance_hash(const LDKTerrainSystem *s)
{
  u64 h = UINT64_C(14695981039346656037);
  h=s_hash_bytes(h,&s->material,sizeof(s->material));
  h=s_hash_bytes(h,&s->surface_count,sizeof(s->surface_count));
  for (u32 i=0;i<s->surface_count;++i)
  {
    float a=s_surface_min_height(s,i), b=s_surface_blend_width(s,i);
    float w=s_surface_texture_weight(s,i);
    rgba32 c=s_surface_color(s,i);
    Vec4 r=s_surface_atlas_rect(s,i);
    Vec2 u=s_surface_uv_scale(s,i);
    h=s_hash_bytes(h,&a,sizeof(a)); h=s_hash_bytes(h,&b,sizeof(b));
    h=s_hash_bytes(h,&w,sizeof(w)); h=s_hash_bytes(h,&c,sizeof(c));
    h=s_hash_bytes(h,&r,sizeof(r)); h=s_hash_bytes(h,&u,sizeof(u));
  }
  h=s_hash_bytes(h,&s->height_scale,sizeof(s->height_scale));
  h=s_hash_bytes(h,&s->height_offset,sizeof(s->height_offset));
  return h;
}

static bool s_chunks_rebuild(LDKTerrainSystem *system, LDKRenderer *renderer)
{
  u32 chunks_x = (s_runtime.source.width + system->chunk_size - 1u) /
      system->chunk_size;
  u32 chunks_y = (s_runtime.source.height + system->chunk_size - 1u) /
      system->chunk_size;
  u64 total64 = (u64)chunks_x * chunks_y;
  if (!total64 || total64 > UINT32_MAX)
    return false;

  s_chunks_destroy(renderer);
  s_runtime.chunks = (LDKTerrainChunk *)calloc(
      (size_t)total64, sizeof(*s_runtime.chunks));
  if (!s_runtime.chunks)
    return false;
  s_runtime.chunk_capacity = (u32)total64;

  float origin_x, origin_z;
  s_world_origin(system, &origin_x, &origin_z);
  u32 chunk_index = 0u;

  for (u32 cy=0; cy<chunks_y; ++cy)
  {
    for (u32 cx=0; cx<chunks_x; ++cx)
    {
      u32 start_x = cx * system->chunk_size;
      u32 start_y = cy * system->chunk_size;
      u32 cells_x = system->chunk_size;
      u32 cells_y = system->chunk_size;
      if (start_x + cells_x > s_runtime.source.width)
        cells_x = s_runtime.source.width - start_x;
      if (start_y + cells_y > s_runtime.source.height)
        cells_y = s_runtime.source.height - start_y;

      u32 vw = cells_x + 1u;
      u32 vh = cells_y + 1u;
      u64 vc64=(u64)vw*vh, ic64=(u64)cells_x*cells_y*6u;
      if (vc64 > UINT32_MAX || ic64 > UINT32_MAX)
      {
        s_chunks_destroy(renderer);
        return false;
      }
      u32 vc=(u32)vc64, ic=(u32)ic64;
      LDKMeshVertex *vertices=(LDKMeshVertex*)calloc(vc,sizeof(*vertices));
      u32 *indices=(u32*)malloc((size_t)ic*sizeof(*indices));
      if (!vertices || !indices)
      {
        free(vertices); free(indices); s_chunks_destroy(renderer); return false;
      }

      Vec3 bmin=vec3_make(FLT_MAX,FLT_MAX,FLT_MAX);
      Vec3 bmax=vec3_make(-FLT_MAX,-FLT_MAX,-FLT_MAX);
      for (u32 y=0;y<vh;++y)
      {
        for (u32 x=0;x<vw;++x)
        {
          u32 gx=start_x+x, gy=start_y+y;
          u32 vi=y*vw+x;
          float wx=origin_x+(float)gx*system->cell_size;
          float wz=origin_z+(float)gy*system->cell_size;
          float wy=s_vertex_world_height(system,gx,gy);
          vertices[vi].position=vec3_make(wx,wy,wz);
          vertices[vi].normal=s_vertex_normal(system,gx,gy);
          vertices[vi].uv=vec2_make(wx,wz);
          vertices[vi].color=LDK_RGBA32(0xffffffffu);
          if(wx<bmin.x)bmin.x=wx; if(wy<bmin.y)bmin.y=wy; if(wz<bmin.z)bmin.z=wz;
          if(wx>bmax.x)bmax.x=wx; if(wy>bmax.y)bmax.y=wy; if(wz>bmax.z)bmax.z=wz;
        }
      }
      u32 ii=0u;
      for(u32 y=0;y<cells_y;++y)
      {
        for(u32 x=0;x<cells_x;++x)
        {
          u32 v0=y*vw+x, v1=v0+1u, v3=(y+1u)*vw+x, v2=v3+1u;
          indices[ii++]=v0; indices[ii++]=v2; indices[ii++]=v1;
          indices[ii++]=v0; indices[ii++]=v3; indices[ii++]=v2;
        }
      }
      LDKRendererMeshDesc desc={0};
      desc.vertices=vertices; desc.vertex_count=vc;
      desc.indices=indices; desc.index_count=ic;
      LDKTerrainChunk *chunk=&s_runtime.chunks[chunk_index++];
      chunk->mesh=ldk_renderer_mesh_create(renderer,&desc);
      chunk->bounds_min=bmin; chunk->bounds_max=bmax;
      free(vertices); free(indices);
      if(!ldk_renderer_mesh_is_valid(renderer,chunk->mesh))
      {
        system->chunk_count=chunk_index;
        s_chunks_destroy(renderer);
        return false;
      }
      system->chunk_count=chunk_index;
    }
  }

  system->data_width=s_runtime.source.width;
  system->data_height=s_runtime.source.height;
  system->mesh_update_count += 1u;
  system->has_geometry=true;
  s_runtime.geometry_hash=s_geometry_hash(system);
  return true;
}

static void s_material_release(LDKRenderer *renderer)
{
  if (!renderer) return;
  ldk_renderer_material_destroy(renderer,s_runtime.material);
  ldk_renderer_image_release(renderer,s_runtime.texture);
  ldk_renderer_image_release(renderer,s_runtime.normal_map);
  ldk_renderer_image_release(renderer,s_runtime.specular_map);
  s_runtime.material=ldk_renderer_material_null();
  s_runtime.texture=ldk_renderer_texture_null();
  s_runtime.normal_map=ldk_renderer_texture_null();
  s_runtime.specular_map=ldk_renderer_texture_null();
  s_runtime.material_asset=ldk_asset_material_null();
  s_runtime.material_revision=0u;
}

static void s_surface_descs(const LDKTerrainSystem *system,
    LDKRendererTerrainSurfaceDesc out[LDK_TERRAIN_SURFACE_COUNT])
{
  memset(out,0,sizeof(*out)*LDK_TERRAIN_SURFACE_COUNT);
  for(u32 i=0;i<system->surface_count;++i)
  {
    out[i].min_height=system->height_offset+
        s_surface_min_height(system,i)*system->height_scale;
    out[i].blend_width=s_surface_blend_width(system,i)*system->height_scale;
    out[i].color=s_surface_color(system,i);
    out[i].atlas_rect=s_surface_atlas_rect(system,i);
    out[i].uv_scale=s_surface_uv_scale(system,i);
    out[i].texture_weight=s_surface_texture_weight(system,i);
  }
}

static bool s_material_resolve(LDKTerrainSystem *system,
    LDKRenderer *renderer, LDKAssetManager *assets)
{
  LDKRendererTerrainSurfaceDesc surfaces[LDK_TERRAIN_SURFACE_COUNT];
  s_surface_descs(system,surfaces);

  const LDKAssetMaterialData *data =
      ldk_asset_manager_material_get_const(assets,system->material);
  u64 revision=data?data->revision:0u;
  u64 hash=s_appearance_hash(system);
  if(ldk_renderer_material_is_valid(renderer,s_runtime.material) &&
      s_runtime.appearance_hash==hash &&
      s_asset_material_equal(s_runtime.material_asset,system->material) &&
      s_runtime.material_revision==revision)
    return true;

  s_material_release(renderer);

  if(data)
  {
    if(!ldk_renderer_terrain_material_resolve(renderer,assets,&data->descriptor,
        surfaces,system->surface_count,&s_runtime.material,&s_runtime.texture,
        &s_runtime.normal_map,&s_runtime.specular_map))
      return false;
    s_runtime.material_asset=system->material;
    s_runtime.material_revision=revision;
  }
  else
  {
    LDKRendererMaterialDesc desc={0};
    desc.type=LDK_MATERIAL_TYPE_VERTEX_COLOR;
    desc.color=LDK_RGBA32(0xffffffffu);
    desc.shininess=32.0f;
    desc.terrain=true;
    desc.terrain_surface_count=system->surface_count;
    memcpy(desc.terrain_surfaces,surfaces,
        (size_t)system->surface_count*sizeof(*surfaces));
    s_runtime.material=ldk_renderer_material_create(renderer,&desc);
    if(!ldk_renderer_material_is_valid(renderer,s_runtime.material))
      return false;
  }
  s_runtime.appearance_hash=hash;
  return true;
}

LDK_API bool ldk_terrain_heightmap_set(LDKAssetImage image)
{
  if (s_runtime.owner)
  {
    s_runtime.owner->heightmap=image;
    s_runtime.geometry_hash=0u;
    return true;
  }
  s_pending_heightmap=image;
  s_pending_heightmap_valid=true;
  return true;
}

LDK_API bool ldk_terrain_system_is_active(void)
{
  return s_runtime.owner != NULL;
}

LDK_API bool ldk_terrain_height_at_world(
    float world_x, float world_z, float *out_height)
{
  LDKTerrainSystem *system=s_runtime.owner;
  if(!system||!out_height||!s_config_is_valid(system)||
      !s_runtime.source.pixels||!isfinite(world_x)||!isfinite(world_z))
    return false;

  float ox,oz; s_world_origin(system,&ox,&oz);
  float lx=(world_x-ox)/system->cell_size;
  float lz=(world_z-oz)/system->cell_size;
  if(lx<0.0f||lz<0.0f||lx>(float)s_runtime.source.width||
      lz>(float)s_runtime.source.height) return false;
  u32 x=(u32)floorf(lx), y=(u32)floorf(lz);
  float tx,tz;
  if(x>=s_runtime.source.width){x=s_runtime.source.width-1u;tx=1.0f;}
  else tx=lx-(float)x;
  if(y>=s_runtime.source.height){y=s_runtime.source.height-1u;tz=1.0f;}
  else tz=lz-(float)y;
  float h00=s_vertex_world_height(system,x,y);
  float h10=s_vertex_world_height(system,x+1u,y);
  float h01=s_vertex_world_height(system,x,y+1u);
  float h11=s_vertex_world_height(system,x+1u,y+1u);
  *out_height = tx>=tz
      ? h00+tx*(h10-h00)+tz*(h11-h10)
      : h00+tz*(h01-h00)+tx*(h11-h01);
  return true;
}

int ldk_terrain_system_initialize(void *data)
{
  LDKTerrainSystem *system=(LDKTerrainSystem*)data;
  if(!system||s_runtime.owner) return -1;
  if(system->cell_size==0.0f)system->cell_size=1.0f;
  if(system->height_scale==0.0f)system->height_scale=1.0f;
  if(system->chunk_size==0u)system->chunk_size=16u;
  if(system->surface_count==0u)system->surface_count=1u;
  s_surface_defaults(system);
  if(s_pending_heightmap_valid)
  {
    system->heightmap=s_pending_heightmap;
    s_pending_heightmap_valid=false;
  }
  if(!s_config_is_valid(system))
  {
    ldk_log_error("Invalid TerrainSystem configuration.\n");
    return -1;
  }
  memset(&s_runtime,0,sizeof(s_runtime));
  s_runtime.owner=system;
  s_runtime.material=ldk_renderer_material_null();
  s_runtime.texture=ldk_renderer_texture_null();
  s_runtime.normal_map=ldk_renderer_texture_null();
  s_runtime.specular_map=ldk_renderer_texture_null();
  s_runtime.material_asset=ldk_asset_material_null();
  system->data_width=system->data_height=0u;
  system->chunk_count=system->visible_chunk_count=0u;
  system->mesh_update_count=0u;
  system->has_geometry=false;
  return 0;
}

void ldk_terrain_system_update(
    void *data, const LDKEntityGroup *group, float dt)
{
  (void)group; (void)dt;
  LDKTerrainSystem *system=(LDKTerrainSystem*)data;
  if(!system||s_runtime.owner!=system||!s_config_is_valid(system)) return;
  LDKRenderer *renderer=(LDKRenderer*)ldk_module_get(LDK_MODULE_RENDERER);
  LDKAssetManager *assets=(LDKAssetManager*)ldk_module_get(LDK_MODULE_ASSET_MANAGER);
  if(!renderer||!assets) return;

  LDKAssetHandle generic={system->heightmap.h};
  const LDKAssetInfo *asset_info=!x_handle_is_null(system->heightmap.h)
      ? ldk_asset_get_info_const(assets,generic):NULL;
  u64 revision=asset_info?asset_info->source_revision:0u;
  bool source_changed=!s_asset_image_equal(s_runtime.source_asset,system->heightmap) ||
      s_runtime.source_revision!=revision || !s_runtime.source.pixels;
  if(source_changed)
  {
    if(!s_source_refresh(system,assets))
    {
      s_chunks_destroy(renderer);
      memset(&s_runtime.source,0,sizeof(s_runtime.source));
      s_runtime.source_asset=ldk_asset_image_null();
      return;
    }
    s_runtime.geometry_hash=0u;
  }

  u64 gh=s_geometry_hash(system);
  if(!system->has_geometry||s_runtime.geometry_hash!=gh)
  {
    if(!s_chunks_rebuild(system,renderer)) return;
  }
  if(!s_material_resolve(system,renderer,assets)) return;

  u32 flags=system->casts_shadows
      ? LDK_RENDERER_MESH_SUBMIT_FLAG_CAST_SHADOWS
      : LDK_RENDERER_MESH_SUBMIT_FLAG_NONE;
  system->visible_chunk_count=0u;
  for(u32 i=0;i<system->chunk_count;++i)
  {
    LDKTerrainChunk *chunk=&s_runtime.chunks[i];
    if(!ldk_renderer_view_bounds_visible(renderer,LDK_RENDERER_VIEW_ALL,
        chunk->bounds_min,chunk->bounds_max))
      continue;
    if(ldk_renderer_submit_mesh_with_flags(renderer,chunk->mesh,
        s_runtime.material,mat4_identity(),flags))
      system->visible_chunk_count++;
  }
}

void ldk_terrain_system_terminate(void *data)
{
  LDKTerrainSystem *system=(LDKTerrainSystem*)data;
  if(!system||s_runtime.owner!=system)return;
  LDKRenderer *renderer=(LDKRenderer*)ldk_module_get(LDK_MODULE_RENDERER);
  if(renderer)
  {
    s_material_release(renderer);
    s_chunks_destroy(renderer);
  }
  else
  {
    free(s_runtime.chunks);
  }
  memset(&s_runtime,0,sizeof(s_runtime));
  system->data_width=system->data_height=0u;
  system->chunk_count=system->visible_chunk_count=0u;
  system->has_geometry=false;
}
