#ifndef LDK_EDITOR_CALLBACKS_INTERNAL_H
#define LDK_EDITOR_CALLBACKS_INTERNAL_H

#include <ldk_game.h>
#include <ldk_scene_systems.h>
#include <module/ldk_ui.h>

void ldki_editor_callbacks_update(LDKGame *game,
    const LDKSceneSystems *systems, float dt);
void ldki_editor_callbacks_draw(LDKGame *game, LDKUIContext *ui,
    const LDKSceneSystems *systems, LDKEntity entity, u64 system_id);

#endif
