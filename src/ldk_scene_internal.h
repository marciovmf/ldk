#ifndef LDK_SCENE_INTERNAL_H
#define LDK_SCENE_INTERNAL_H

#include <ldk_scene.h>
#include <module/ldk_entity.h>

LDKEntity ldki_prefab_instantiate_tml(
    const char *source, LDKSceneResult *result);

#endif // LDK_SCENE_INTERNAL_H
