/**
 * # Component editor metadata
 *
 * Inspect engine-registered component metadata used for editor controls.
 */

#ifndef LDK_COMPONENT_META_H
#define LDK_COMPONENT_META_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group ldk.editor.component_metadata */
  typedef enum LDKFieldType
  {
    LDK_FIELD_BOOL = 0,
    LDK_FIELD_I32,
    LDK_FIELD_U32,
    LDK_FIELD_FLOAT,
    LDK_FIELD_VEC2,
    LDK_FIELD_VEC3,
    LDK_FIELD_VEC4,
    LDK_FIELD_QUAT,
    LDK_FIELD_MAT4,
    LDK_FIELD_ENUM,
    LDK_FIELD_ENTITY,
    LDK_FIELD_ASSET_MESH,
    LDK_FIELD_ASSET_FONT,
    LDK_FIELD_ASSET_IMAGE,
    LDK_FIELD_RESOURCE_MESH,
    LDK_FIELD_ASSET_MATERIAL,
    LDK_FIELD_ASSET_SKYBOX,
    LDK_FIELD_STRING,
    LDK_FIELD_ASSET_AUDIO,
    LDK_FIELD_ASSET_KEYFRAME_ANIMATION,
    LDK_FIELD_ASSET_TEXTURE_ATLAS,
    LDK_FIELD_U64,
    LDK_FIELD_ARRAY,
  } LDKFieldType;

  /** @group ldk.editor.component_metadata */
  typedef enum LDKFieldWidget
  {
    LDK_FIELD_WIDGET_DEFAULT = 0,
    LDK_FIELD_WIDGET_CHECKBOX,
    LDK_FIELD_WIDGET_I32,
    LDK_FIELD_WIDGET_U32,
    LDK_FIELD_WIDGET_FLOAT,
    LDK_FIELD_WIDGET_SLIDER,
    LDK_FIELD_WIDGET_VEC2,
    LDK_FIELD_WIDGET_VEC3,
    LDK_FIELD_WIDGET_VEC4,
    LDK_FIELD_WIDGET_QUAT,
    LDK_FIELD_WIDGET_MAT4,
    LDK_FIELD_WIDGET_ENUM,
    LDK_FIELD_WIDGET_ENTITY,
    LDK_FIELD_WIDGET_ASSET_MESH,
    LDK_FIELD_WIDGET_ASSET_FONT,
    LDK_FIELD_WIDGET_ASSET_IMAGE,
    LDK_FIELD_WIDGET_RESOURCE_MESH,
    LDK_FIELD_WIDGET_EULER,
    LDK_FIELD_WIDGET_COLOR,
    LDK_FIELD_WIDGET_ASSET_MATERIAL,
    LDK_FIELD_WIDGET_ASSET_SKYBOX,
    LDK_FIELD_WIDGET_STRING,
    LDK_FIELD_WIDGET_ASSET_AUDIO,
    LDK_FIELD_WIDGET_ASSET_KEYFRAME_ANIMATION,
    LDK_FIELD_WIDGET_ASSET_TEXTURE_ATLAS,
    LDK_FIELD_WIDGET_ATLAS_REGION,
  } LDKFieldWidget;

  /** @group ldk.editor.component_metadata */
  typedef enum LDKFieldFlags
  {
    LDK_FIELD_FLAG_NONE = 0,
    LDK_FIELD_FLAG_READONLY = 1 << 0,
    LDK_FIELD_FLAG_RUNTIME = 1 << 1,
    LDK_FIELD_FLAG_HEADLINE = 1 << 2, /* Use field value as collapsed group title. */
  } LDKFieldFlags;

  /** @group ldk.editor.component_metadata */
  typedef struct LDKEnumOption
  {
    const char *name; /* Persistent C identifier. */
    const char *label;
    i64 value;
  } LDKEnumOption;

  /** @group ldk.editor.component_metadata */
  typedef struct LDKEnumMeta
  {
    const char *name;
    const LDKEnumOption *options;
    u32 count;
    u32 size;
    u32 alignment;
    i64 (*read)(const void *value);
    void (*write)(void *value, i64 raw);
  } LDKEnumMeta;

  /** @group ldk.editor.component_metadata */
  typedef struct LDKComponentGroupMeta
  {
    const char *name;
    const struct LDKComponentGroupMeta *parent;
  } LDKComponentGroupMeta;

  struct LDKComponentMeta;

  /* Optional type reflection and hooks for STDX XArray fields. */
  typedef struct LDKArrayFieldMeta
  {
    const struct LDKComponentMeta *(*element_meta)(void);
    void (*element_initialize)(void *element);
    void (*on_change)(void *component);
    const char *limit_field; /* Optional sibling u32 field name. */
    u32 max_count; /* Zero means no additional fixed maximum. */
    bool stable; /* Only append and remove final element; no reordering. */
  } LDKArrayFieldMeta;

  /** @group ldk.editor.component_metadata */
  typedef struct LDKComponentFieldMeta
  {
    const char *name;
    LDKFieldType type;
    u32 offset;
    u32 flags;
    LDKFieldWidget widget;
    float min_value;
    float max_value;
    const LDKEnumMeta *enum_meta;
    const LDKComponentGroupMeta *group;
    const char *display_name;
    const char *widget_source; /* Name of a sibling field providing widget data. */
    const LDKArrayFieldMeta *array_meta;
  } LDKComponentFieldMeta;

  /** @group ldk.editor.component_metadata */
  typedef struct LDKComponentMeta
  {
    const char *name;
    u32 type;
    u32 size;
    const LDKComponentFieldMeta *fields;
    u32 field_count;
    const LDKComponentGroupMeta *groups;
    u32 group_count;
  } LDKComponentMeta;

  /**
   * @brief Count the engine component metadata.
   * @return Number of matching entries.
   * @group ldk.editor.component_metadata
   */
  LDK_API u32 ldk_engine_component_metadata_count(void);
  /**
   * @brief Get the engine component metadata.
   * @param index Zero-based index of the selected item.
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group ldk.editor.component_metadata
   */
  LDK_API const LDKComponentMeta *ldk_engine_component_metadata_get(
      u32 index);

#ifdef __cplusplus
}
#endif

#endif
