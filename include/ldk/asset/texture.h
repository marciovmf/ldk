#ifndef TEXTURE_H
#define TEXTURE_H

#include "../common.h"
#include "../module/renderer.h"


LDK_API LDKHTexture ldkAssetTextureLoadFunc(const char* path);
LDK_API void ldkAssetTextureUnloadFunc(LDKHTexture handle);

#endif //TEXTURE_H

