/* Reflected, validated property access shared by editor and runtime. */
#ifndef LDK_PROPERTY_H
#define LDK_PROPERTY_H

#include <ldk_common.h>
#include <editor/ldk_component_metadata.h>
#include <module/ldk_entity.h>
#include <stdx/stdx_math.h>
#include <stdx/stdx_string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LDKPropertyValue
{
  LDKFieldType type;
  Vec4 vector;  /* FLOAT/VEC2/VEC3/VEC4/QUAT: used components in order. */
  i64 integer; /* BOOL/I32/U32/ENUM and packed RGBA32. */
  XSmallstr string;
} LDKPropertyValue;

/* Hidden fields are absent from Comet metadata. Readonly fields are excluded.
 * Asset/entity/resource handles and matrices have no stable value codec yet. */
LDK_API bool ldk_property_field_supported(const LDKComponentFieldMeta *field);
LDK_API const LDKComponentMeta *ldk_property_component_meta(u32 component_type);
LDK_API const LDKComponentFieldMeta *ldk_property_field_find(
    u32 component_type, const char *field_name);
LDK_API bool ldk_property_get(LDKEntity entity, u32 component_type,
    const char *field_name, LDKPropertyValue *out);
/* Register an optional writer for fields with side effects. A failing writer
 * returns false and never falls through to a raw memory write. Registrations
 * are cleared before unloading a game DLL. */
typedef bool (*LDKPropertyWriterFn)(LDKEntity entity,
    const LDKPropertyValue *value, void *user);
LDK_API bool ldk_property_writer_register(u32 component_type,
    const char *field_name, LDKPropertyWriterFn callback, void *user);
LDK_API void ldk_property_writers_clear(void);
LDK_API bool ldk_property_set(LDKEntity entity, u32 component_type,
    const char *field_name, const LDKPropertyValue *value);
LDK_API bool ldk_property_interpolate(
    const LDKComponentFieldMeta *field, const LDKPropertyValue *a,
    const LDKPropertyValue *b, float alpha, LDKPropertyValue *out);

#ifdef __cplusplus
}
#endif
#endif
