#ifndef LDK_COMPONENT_META_H
#define LDK_COMPONENT_META_H

#include <ldk_common.h>

#ifdef __cplusplus
extern "C" {
#endif

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
  LDK_FIELD_RESOURCE_MESH,
} LDKFieldType;

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
  LDK_FIELD_WIDGET_RESOURCE_MESH,
} LDKFieldWidget;

typedef enum LDKFieldFlags
{
  LDK_FIELD_FLAG_NONE     = 0,
  LDK_FIELD_FLAG_READONLY = 1 << 0,
  LDK_FIELD_FLAG_RUNTIME  = 1 << 1,
} LDKFieldFlags;

typedef struct LDKComponentFieldMeta
{
  const char* name;
  LDKFieldType type;
  u32 offset;
  u32 flags;
  LDKFieldWidget widget;
  float min_value;
  float max_value;
} LDKComponentFieldMeta;

typedef struct LDKComponentMeta
{
  const char* name;
  u32 type;
  u32 size;
  const LDKComponentFieldMeta* fields;
  u32 field_count;
} LDKComponentMeta;

#ifdef __cplusplus
}
#endif

#endif
