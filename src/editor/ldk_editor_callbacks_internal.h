#ifndef LDK_EDITOR_CALLBACKS_INTERNAL_H
#define LDK_EDITOR_CALLBACKS_INTERNAL_H

#include <ldk_game.h>
#include <ldk_scene_systems.h>
#include <module/ldk_ui.h>

void ldki_editor_callbacks_update(LDKGame *game,
    const LDKSceneSystems *systems, float dt);
/* Draw only actions belonging to the specified component or system.
 * Returns true if an action was invoked and the Inspector should stop drawing. */
bool ldki_editor_callbacks_draw(LDKGame *game, LDKUIContext *ui,
    const LDKSceneSystems *systems, LDKEntity entity, u64 system_id,
    u32 component_type);

#endif
