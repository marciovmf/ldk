#ifndef LDK_SYSTEM_METADATA_H
#define LDK_SYSTEM_METADATA_H

#include <ldk_common.h>
#include <editor/ldk_component_metadata.h>

typedef struct LDKSystemMeta
{
  const char *name;
  u64 id;
  /* Stateful metadata uses the same field descriptions as components.
   * Stateless systems leave size, fields and field_count zero. */
  u32 size;
  const LDKComponentFieldMeta *fields;
  u32 field_count;
} LDKSystemMeta;

#endif

