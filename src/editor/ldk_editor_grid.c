#include "ldk_editor_internal.h"

#include <component/ldk_camera.h>

#define LDK_EDITOR_GRID_EXTENT 512.0f
#define LDK_EDITOR_GRID_SPACING 1.0f

void ldki_editor_grid_submit(LDKEditorContext *editor)
{
  Mat4 camera_world;
  Vec3 grid_center;

  if (editor == NULL || editor->renderer == NULL ||
      editor->scene_view == LDK_RENDERER_VIEW_INVALID ||
      editor->scene_view == LDK_RENDERER_VIEW_ALL ||
      !ldk_camera_get_world_matrix(editor->editor_camera, &camera_world))
  {
    return;
  }

  grid_center = vec3_make(camera_world.m[12], 0.0f, camera_world.m[14]);
  ldk_renderer_submit_grid_to_view(editor->renderer, editor->scene_view,
      grid_center, LDK_EDITOR_GRID_EXTENT, LDK_EDITOR_GRID_SPACING);
}
