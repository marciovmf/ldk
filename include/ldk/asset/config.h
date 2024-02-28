/**
 * config.h
 *
 * Asset handler for .config assets
 *
 */

#ifndef LDK_CONFIG_H
#define LDK_CONFIG_H

#include "../common.h"
#include "../hashmap.h"
#include "../maths.h"
#include "../module/asset.h"

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum
  {
    LDK_CONFIG_VAR_TYPE_UNKNOWN = 0x00,
    LDK_CONFIG_VAR_TYPE_DOUBLE  = 0x01,
    LDK_CONFIG_VAR_TYPE_LONG    = 0x02,
    LDK_CONFIG_VAR_TYPE_VEC2    = 0x03,
    LDK_CONFIG_VAR_TYPE_VEC3    = 0x04,
    LDK_CONFIG_VAR_TYPE_VEC4    = 0x05,
    LDK_CONFIG_VAR_TYPE_STRING  = 0x06,
  } LDKConfigVaryableType;

  typedef struct
  {
    const char* name;
    LDKConfigVaryableType type;
    union
    {
      int64 longValue;
      double doubleValue;
      Vec2 vec2Value;
      Vec3 vec3Value;
      Vec4 vec4Value;
      const char* strValue;
    };

  } LDKConfigVariable;

  typedef struct 
  {
    LDK_DECLARE_ASSET;
    LDKHashMap* map;
    char* buffer;
    bool raw;
  } LDKConfig;

  typedef LDKHandle LDKHImage;


  LDK_API bool ldkAssetConfigLoadFunc(const char* path, LDKAsset asset);
  LDK_API void ldkAssetConfigUnloadFunc(LDKAsset asset);

  LDK_API LDKConfigVariable* ldkConfigGetVariable(LDKConfig* config, LDKTypeId typeId);
  LDK_API int32 ldkConfigGetInt(LDKConfig* config, const char* name);
  LDK_API int64 ldkConfigGetLong(LDKConfig* config, const char* name);
  LDK_API double ldkConfigGetDouble(LDKConfig* config, const char* name);
  LDK_API float ldkConfigGetFloat(LDKConfig* config, const char* name);
  LDK_API const char* ldkConfigGetString(LDKConfig* config, const char* name);
  LDK_API Vec2 ldkConfigGetVec2(LDKConfig* config, const char* name);
  LDK_API Vec3 ldkConfigGetVec3(LDKConfig* config, const char* name);
  LDK_API Vec4 ldkConfigGetVec4(LDKConfig* config, const char* name);

#ifdef __cplusplus
}
#endif

#endif //LDK_CONFIG_H

