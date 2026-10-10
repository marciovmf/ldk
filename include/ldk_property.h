/**
 * @file ldk_property.h
 * @brief Shared runtime/editor access to reflected component properties.
 */
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

/**
 * @brief Check whether a reflected field is writable and has an animation value codec.
 * @param field Metadata describing the property.
 * @return True for supported non-readonly fields; hidden fields are absent from metadata.
 */
LDK_API bool ldk_property_field_supported(const LDKComponentFieldMeta *field);
/**
 * @brief Find component metadata using the runtime ECS component type.
 * @param component_type Runtime ECS component type.
 * @return Metadata pointer, or NULL if no compatible registration exists.
 */
LDK_API const LDKComponentMeta *ldk_property_component_meta(u32 component_type);
/**
 * @brief Find a reflected field in runtime component metadata.
 * @param component_type Runtime ECS component type.
 * @param field_name Persistent reflected property name.
 * @return Field metadata pointer, or NULL when unavailable.
 */
LDK_API const LDKComponentFieldMeta *ldk_property_field_find(
    u32 component_type, const char *field_name);
/**
 * @brief Read a supported component field into a type-tagged value.
 * @param entity Target ECS entity.
 * @param component_type Runtime ECS component type.
 * @param field_name Persistent reflected property name.
 * @param out Receives the type-tagged property value.
 * @return True when the entity has the component and the reflected value is valid.
 */
LDK_API bool ldk_property_get(LDKEntity entity, u32 component_type,
    const char *field_name, LDKPropertyValue *out);
/* Register an optional writer for fields with side effects. A failing writer
 * returns false and never falls through to a raw memory write. Registrations
 * are cleared before unloading a game DLL. */
/**
 * @brief Specialized setter for a reflected field requiring side effects.
 * @param entity Entity containing the property.
 * @param value Validated value to apply.
 * @param user Opaque registration context.
 * @return True on successful application, false on failure.
 */
typedef bool (*LDKPropertyWriterFn)(LDKEntity entity,
    const LDKPropertyValue *value, void *user);
/**
 * @brief Register or replace a specialized setter for a reflected field.
 * @param component_type Runtime ECS component type.
 * @param field_name Persistent reflected property name.
 * @param callback Function responsible for writing the value.
 * @param user Opaque callback context.
 * @return True on success. Writer must apply the value and return its status.
 */
LDK_API bool ldk_property_writer_register(u32 component_type,
    const char *field_name, LDKPropertyWriterFn callback, void *user);
/**
 * @brief Clear registered property writer callbacks before unloading their code.
 * @return Nothing. Must be called before unloading game-module callbacks.
 */
LDK_API void ldk_property_writers_clear(void);
/**
 * @brief Validate and assign a component field using a registered writer or native setter.
 * @param entity Target ECS entity.
 * @param component_type Runtime ECS component type.
 * @param field_name Persistent reflected property name.
 * @param value Type-tagged property value.
 * @return True if the field accepted the value. Unchanged values do not invoke setters.
 */
LDK_API bool ldk_property_set(LDKEntity entity, u32 component_type,
    const char *field_name, const LDKPropertyValue *value);
/**
 * @brief Interpolate or step between two supported typed property values.
 * @param field Metadata describing the property.
 * @param a First property value.
 * @param b Second property value.
 * @param alpha Interpolation factor in [0,1].
 * @param out Receives the interpolated type-tagged value.
 * @return True when both values match the reflected field and are valid.
 */
LDK_API bool ldk_property_interpolate(
    const LDKComponentFieldMeta *field, const LDKPropertyValue *a,
    const LDKPropertyValue *b, float alpha, LDKPropertyValue *out);

#ifdef __cplusplus
}
#endif
#endif
