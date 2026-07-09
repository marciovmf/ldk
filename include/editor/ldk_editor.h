#ifndef LDK_EDITOR_H
#define LDK_EDITOR_H

#include <ldk_common.h>
#include <module/ldk_ecs.h>
#include <module/ldk_ui.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef void LDKEditor;

  //------------------------------------------------------------
  // Core Editor functions
  //------------------------------------------------------------

  /**
   * Returns an instance of the editor
   */
  LDKEditor *ldk_editor_get(void);

  /**
   * Shows the file explorer window
   */
  void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path);

  /**
   * Shows the console window
   */
  void ldk_editor_console_show(LDKEditor *editor);

  /**
   * Shows the hierarchy window
   */
  void ldk_editor_hierarchy_show(LDKEditor *editor, LDKECS *ecs);

  /**
   * Enter PLAY mode
   */
  void ldk_editor_state_set_play(LDKEditor *editor);

  /**
   * Exists PLAY mode
   */
  void ldk_editor_state_set_stop(LDKEditor *editor);

  /**
   * Pauses PLAY mode
   */
  void ldk_editor_state_set_pause(LDKEditor *editor);

  /**
   * Enters PlayMode for exactly one frame.
   */
  void ldk_editor_state_play_one_frame(LDKEditor *editor);

  /**
   * Loads a project
   */
  bool ldk_editor_project_load(LDKEditor *editor, const char *project_path);

  /**
   * Quits the editor
   */
  void ldk_editor_quit(LDKEditor *editor);


  //------------------------------------------------------------
  // Command System interface
  //------------------------------------------------------------

  typedef bool (*LDKEditorCommandFn)(XSlice args);
  /**
   * Registers an editor command
   */
  bool ldk_editor_command_register(LDKEditor *editor, const char *cmd_name,
      const char *help, LDKEditorCommandFn fn);

  /**
   * Unregister an editor command
   */
  bool ldk_editor_command_unregister(LDKEditor *editor, const char *cmd_name);

  /**
   * Runs a command
   */
  bool ldk_editor_command_run(LDKEditor *editor, const char *cmd_with_args);

#ifdef __cplusplus
}
#endif

#endif // LDK_EDITOR_H
