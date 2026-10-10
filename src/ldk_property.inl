/* Included from ldk.c; engine and game metadata are resolved at call time. */
#include <ldk_property.h>
#include <ldk_game.h>
#include <ldk_scene.h>
#include <component/ldk_transform.h>
#include <module/ldk_ecs.h>
#include <math.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

const LDKComponentMeta *ldk_property_component_meta(u32 component_type)
{
  LDKGame *game = &g_engine.game;
  u32 count = game->metadata_count && game->metadata_get ?
      game->metadata_count() : ldk_engine_component_metadata_count();
  for (u32 i = 0; i < count; ++i)
  {
    const LDKComponentMeta *meta = game->metadata_count && game->metadata_get ?
        game->metadata_get(i) : ldk_engine_component_metadata_get(i);
    if (meta &&
        ldk_scene_component_meta_runtime_type(meta) == component_type)
    {
      return meta;
    }
  }
  return NULL;
}

const LDKComponentFieldMeta *ldk_property_field_find(
    u32 component_type, const char *field_name)
{
  const LDKComponentMeta *meta = ldk_property_component_meta(component_type);
  if (!meta || !field_name)
  {
    return NULL;
  }
  for (u32 i = 0; i < meta->field_count; ++i)
  {
    if (strcmp(meta->fields[i].name, field_name) == 0)
    {
      return &meta->fields[i];
    }
  }
  return NULL;
}

bool ldk_property_field_supported(const LDKComponentFieldMeta *field)
{
  if (!field || (field->flags & LDK_FIELD_FLAG_READONLY))
  {
    return false;
  }
  switch (field->type)
  {
  case LDK_FIELD_BOOL:
  case LDK_FIELD_I32:
  case LDK_FIELD_U32:
  case LDK_FIELD_FLOAT:
  case LDK_FIELD_VEC2:
  case LDK_FIELD_VEC3:
  case LDK_FIELD_VEC4:
  case LDK_FIELD_QUAT:
  case LDK_FIELD_STRING:
    return true;
  case LDK_FIELD_ENUM:
    return field->enum_meta && field->enum_meta->read && field->enum_meta->write;
  default:
    return false;
  }
}

static bool s_property_value_valid(const LDKComponentFieldMeta *field,
    const LDKPropertyValue *value)
{
  if (!ldk_property_field_supported(field) || !value ||
      value->type != field->type)
  {
    return false;
  }
  switch (value->type)
  {
  case LDK_FIELD_FLOAT:
  case LDK_FIELD_VEC2:
  case LDK_FIELD_VEC3:
  case LDK_FIELD_VEC4:
  case LDK_FIELD_QUAT:
    return isfinite(value->vector.x) && isfinite(value->vector.y) &&
        isfinite(value->vector.z) && isfinite(value->vector.w);
  case LDK_FIELD_U32:
    return value->integer >= 0 && value->integer <= UINT32_MAX;
  case LDK_FIELD_I32:
    return value->integer >= INT32_MIN && value->integer <= INT32_MAX;
  case LDK_FIELD_BOOL:
    return value->integer == 0 || value->integer == 1;
  case LDK_FIELD_STRING:
    return value->string.length <= X_SMALLSTR_MAX_LENGTH &&
        value->string.buf[value->string.length] == 0 &&
        strlen(value->string.buf) == value->string.length;
  default:
    return true;
  }
}

bool ldk_property_get(LDKEntity entity, u32 component_type,
    const char *field_name, LDKPropertyValue *out)
{
  const LDKComponentFieldMeta *field =
      ldk_property_field_find(component_type, field_name);
  const LDKComponentMeta *meta = ldk_property_component_meta(component_type);
  const void *component = ldk_ecs_component_get_const(entity, component_type);
  if (!out || !component || !meta || !ldk_property_field_supported(field))
  {
    return false;
  }
  size_t size = field->type == LDK_FIELD_STRING ? sizeof(XSmallstr) :
      field->type == LDK_FIELD_VEC2 ? sizeof(Vec2) :
      field->type == LDK_FIELD_VEC3 ? sizeof(Vec3) :
      field->type == LDK_FIELD_VEC4 ? sizeof(Vec4) :
      field->type == LDK_FIELD_QUAT ? sizeof(Quat) :
      field->type == LDK_FIELD_FLOAT ? sizeof(float) :
      field->type == LDK_FIELD_BOOL ? sizeof(bool) : sizeof(u32);
  if (field->type == LDK_FIELD_ENUM)
  {
    size = field->enum_meta->size;
  }
  if (size > meta->size || field->offset > meta->size - size)
  {
    return false;
  }
  const char *p = (const char *)component + field->offset;
  LDKPropertyValue v = {0};
  v.type = field->type;
  switch (field->type)
  {
  case LDK_FIELD_BOOL: v.integer = *(const bool *)p; break;
  case LDK_FIELD_I32: v.integer = *(const i32 *)p; break;
  case LDK_FIELD_U32: v.integer = *(const u32 *)p; break;
  case LDK_FIELD_ENUM: v.integer = field->enum_meta->read(p); break;
  case LDK_FIELD_FLOAT: v.vector.x = *(const float *)p; break;
  case LDK_FIELD_VEC2:
  {
    const Vec2 *vec = (const Vec2 *)p;
    v.vector.x = vec->x;
    v.vector.y = vec->y;
    break;
  }
  case LDK_FIELD_VEC3:
  {
    const Vec3 *vec = (const Vec3 *)p;
    v.vector.x = vec->x;
    v.vector.y = vec->y;
    v.vector.z = vec->z;
    break;
  }
  case LDK_FIELD_VEC4: v.vector = *(const Vec4 *)p; break;
  case LDK_FIELD_QUAT:
  {
    const Quat *q = (const Quat *)p;
    v.vector = vec4_make(q->x, q->y, q->z, q->w);
    break;
  }
  case LDK_FIELD_STRING: v.string = *(const XSmallstr *)p; break;
  default: return false;
  }
  if (!s_property_value_valid(field, &v))
  {
    return false;
  }
  *out = v;
  return true;
}

typedef struct LDKPropertyWriter
{
  u32 component_type;
  char field_name[96];
  LDKPropertyWriterFn callback;
  void *user;
} LDKPropertyWriter;

static LDKPropertyWriter *s_property_writers;
static u32 s_property_writer_count;

bool ldk_property_writer_register(u32 component_type,
    const char *field_name, LDKPropertyWriterFn callback, void *user)
{
  if (!callback || !field_name || !field_name[0] ||
      strlen(field_name) >= sizeof(s_property_writers[0].field_name) ||
      !ldk_property_field_supported(
          ldk_property_field_find(component_type, field_name)))
  {
    return false;
  }
  for (u32 i = 0; i < s_property_writer_count; ++i)
  {
    LDKPropertyWriter *w = &s_property_writers[i];
    if (w->component_type == component_type &&
        strcmp(w->field_name, field_name) == 0)
    {
      w->callback = callback;
      w->user = user;
      return true;
    }
  }
  if (s_property_writer_count == UINT32_MAX ||
      (size_t)(s_property_writer_count + 1u) >
          SIZE_MAX / sizeof(*s_property_writers))
  {
    return false;
  }
  void *memory = realloc(s_property_writers,
      (s_property_writer_count + 1u) * sizeof(*s_property_writers));
  if (!memory)
  {
    return false;
  }
  s_property_writers = memory;
  LDKPropertyWriter *w = &s_property_writers[s_property_writer_count++];
  *w = (LDKPropertyWriter){0};
  w->component_type = component_type;
  memcpy(w->field_name, field_name, strlen(field_name) + 1);
  w->callback = callback;
  w->user = user;
  return true;
}

void ldk_property_writers_clear(void)
{
  free(s_property_writers);
  s_property_writers = NULL;
  s_property_writer_count = 0;
}

bool ldk_property_set(LDKEntity entity, u32 component_type,
    const char *field_name, const LDKPropertyValue *value)
{
  const LDKComponentFieldMeta *field =
      ldk_property_field_find(component_type, field_name);
  const LDKComponentMeta *meta = ldk_property_component_meta(component_type);
  void *component = ldk_ecs_component_get(entity, component_type);
  if (!component || !meta || !s_property_value_valid(field, value))
  {
    return false;
  }
  size_t size = value->type == LDK_FIELD_STRING ? sizeof(XSmallstr) :
      value->type == LDK_FIELD_VEC2 ? sizeof(Vec2) :
      value->type == LDK_FIELD_VEC3 ? sizeof(Vec3) :
      value->type == LDK_FIELD_VEC4 ? sizeof(Vec4) :
      value->type == LDK_FIELD_QUAT ? sizeof(Quat) :
      value->type == LDK_FIELD_FLOAT ? sizeof(float) :
      value->type == LDK_FIELD_BOOL ? sizeof(bool) : sizeof(u32);
  if (value->type == LDK_FIELD_ENUM)
  {
    size = field->enum_meta->size;
  }
  if (size > meta->size || field->offset > meta->size - size)
  {
    return false;
  }
  /* Step tracks should not repeatedly trigger setters or component
   * callbacks when the discrete value has not changed. */
  LDKPropertyValue previous;
  if (ldk_property_get(entity, component_type, field_name, &previous))
  {
    bool equal = false;
    switch (value->type)
    {
    case LDK_FIELD_BOOL:
    case LDK_FIELD_I32:
    case LDK_FIELD_U32:
    case LDK_FIELD_ENUM:
      equal = previous.integer == value->integer;
      break;
    case LDK_FIELD_STRING:
      equal = previous.string.length == value->string.length &&
          memcmp(previous.string.buf, value->string.buf,
              previous.string.length + 1) == 0;
      break;
    case LDK_FIELD_FLOAT:
      equal = previous.vector.x == value->vector.x;
      break;
    case LDK_FIELD_VEC2:
      equal = previous.vector.x == value->vector.x &&
          previous.vector.y == value->vector.y;
      break;
    case LDK_FIELD_VEC3:
      equal = previous.vector.x == value->vector.x &&
          previous.vector.y == value->vector.y &&
          previous.vector.z == value->vector.z;
      break;
    case LDK_FIELD_VEC4:
    case LDK_FIELD_QUAT:
      equal = previous.vector.x == value->vector.x &&
          previous.vector.y == value->vector.y &&
          previous.vector.z == value->vector.z &&
          previous.vector.w == value->vector.w;
      break;
    default:
      break;
    }
    if (equal)
    {
      return true;
    }
  }
  for (u32 i = 0; i < s_property_writer_count; ++i)
  {
    LDKPropertyWriter *w = &s_property_writers[i];
    if (w->component_type == component_type &&
        strcmp(w->field_name, field_name) == 0)
    {
      return w->callback(entity, value, w->user);
    }
  }
  char *p = (char *)component + field->offset;
  if (component_type == LDK_COMPONENT_TYPE_TRANSFORM)
  {
    if (strcmp(field->name, "local_position") == 0 &&
        value->type == LDK_FIELD_VEC3)
    {
      return ldk_transform_set_local_position(entity,
          vec3_make(value->vector.x, value->vector.y, value->vector.z));
    }
    if (strcmp(field->name, "local_scale") == 0 &&
        value->type == LDK_FIELD_VEC3)
    {
      return ldk_transform_set_local_scale(entity,
          vec3_make(value->vector.x, value->vector.y, value->vector.z));
    }
    if (strcmp(field->name, "local_rotation") == 0 &&
        value->type == LDK_FIELD_QUAT)
    {
      return ldk_transform_set_local_rotation(entity, quat_norm(quat_make(
          value->vector.x, value->vector.y, value->vector.z,
          value->vector.w)));
    }
  }
  switch (value->type)
  {
  case LDK_FIELD_BOOL: *(bool *)p = value->integer != 0; break;
  case LDK_FIELD_I32: *(i32 *)p = (i32)value->integer; break;
  case LDK_FIELD_U32: *(u32 *)p = (u32)value->integer; break;
  case LDK_FIELD_ENUM: field->enum_meta->write(p, value->integer); break;
  case LDK_FIELD_FLOAT: *(float *)p = value->vector.x; break;
  case LDK_FIELD_VEC2: *(Vec2 *)p = vec2_make(value->vector.x, value->vector.y); break;
  case LDK_FIELD_VEC3: *(Vec3 *)p = vec3_make(value->vector.x,
      value->vector.y, value->vector.z); break;
  case LDK_FIELD_VEC4: *(Vec4 *)p = value->vector; break;
  case LDK_FIELD_QUAT: *(Quat *)p = quat_make(value->vector.x,
      value->vector.y, value->vector.z, value->vector.w); break;
  case LDK_FIELD_STRING: *(XSmallstr *)p = value->string; break;
  default: return false;
  }
  return true;
}

bool ldk_property_interpolate(const LDKComponentFieldMeta *field,
    const LDKPropertyValue *a, const LDKPropertyValue *b,
    float alpha, LDKPropertyValue *out)
{
  if (!out || !s_property_value_valid(field, a) ||
      !s_property_value_valid(field, b) || !isfinite(alpha))
  {
    return false;
  }
  alpha = fmaxf(0.0f, fminf(1.0f, alpha));
  *out = *a;
  if (a->type == LDK_FIELD_FLOAT || a->type == LDK_FIELD_VEC2 ||
      a->type == LDK_FIELD_VEC3 || a->type == LDK_FIELD_VEC4)
  {
    out->vector = vec4_lerp(a->vector, b->vector, alpha);
  }
  else if (a->type == LDK_FIELD_QUAT)
  {
    Quat qa = quat_make(a->vector.x, a->vector.y, a->vector.z, a->vector.w);
    Quat qb = quat_make(b->vector.x, b->vector.y, b->vector.z, b->vector.w);
    float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
    if (dot < 0.0f)
    {
      qb = quat_make(-qb.x, -qb.y, -qb.z, -qb.w);
    }
    Quat q = quat_norm(quat_make(qa.x + (qb.x - qa.x) * alpha,
        qa.y + (qb.y - qa.y) * alpha,
        qa.z + (qb.z - qa.z) * alpha,
        qa.w + (qb.w - qa.w) * alpha));
    out->vector = vec4_make(q.x, q.y, q.z, q.w);
  }
  else if (a->type == LDK_FIELD_U32 && field->widget == LDK_FIELD_WIDGET_COLOR)
  {
    u32 ca = (u32)a->integer;
    u32 cb = (u32)b->integer;
    u32 result = 0;
    for (u32 i = 0; i < 4; ++i)
    {
      u32 shift = i * 8;
      float va = (float)((ca >> shift) & 255u);
      float vb = (float)((cb >> shift) & 255u);
      result |= (u32)lroundf(va + (vb - va) * alpha) << shift;
    }
    out->integer = result;
  }
  else if (a->type == LDK_FIELD_I32 || a->type == LDK_FIELD_U32)
  {
    double delta = (double)b->integer - (double)a->integer;
    out->integer = (i64)llround((double)a->integer + delta * alpha);
  }
  else if (alpha >= 1.0f)
  {
    *out = *b;
  }
  return true;
}
