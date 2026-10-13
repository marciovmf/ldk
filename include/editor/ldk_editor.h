/**
 * # Editor commands and state
 *
 * Control editor windows, project loading, play mode, and editor console commands.
 */

#ifndef LDK_EDITOR_H
#define LDK_EDITOR_H

#include <ldk_common.h>
#include <module/ldk_ecs.h>
#include <module/ldk_ui.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @group editor.editor.lifecycle */
  typedef void LDKEditor;

  //------------------------------------------------------------
  // Core Editor functions
  //------------------------------------------------------------

  /**
   * Returns an instance of the editor
   * @return Pointer to the requested object or data; NULL when unavailable.
   * @group editor.editor.lifecycle
   */
  LDKEditor *ldk_editor_get(void);

  /**
   * Shows the file explorer window
   * @param editor Editor instance.
   * @param root_path Filesystem root shown by the explorer.
   * @group editor.editor.windows
   */
  void ldk_editor_file_explorer_show(LDKEditor *editor, const char *root_path);

  /**
   * Shows the console window
   * @param editor Editor instance.
   * @group editor.editor.windows
   */
  void ldk_editor_console_show(LDKEditor *editor);

  /**
   * Shows the hierarchy window
   * @param editor Editor instance.
   * @param ecs Entity-component-system context.
   * @group editor.editor.windows
   */
  void ldk_editor_hierarchy_show(LDKEditor *editor, LDKECS *ecs);

  /**
   * Shows the profiler window
   * @param editor Editor instance.
   * @group editor.editor.windows
   */
  void ldk_editor_profiler_show(LDKEditor *editor);

  /**
   * Enter PLAY mode
   * @param editor Editor instance.
   * @group editor.editor.play_mode
   */
  void ldk_editor_state_set_play(LDKEditor *editor);

  /**
   * Exists PLAY mode
   * @param editor Editor instance.
   * @group editor.editor.play_mode
   */
  void ldk_editor_state_set_stop(LDKEditor *editor);

  /**
   * Pauses PLAY mode
   * @param editor Editor instance.
   * @group editor.editor.play_mode
   */
  void ldk_editor_state_set_pause(LDKEditor *editor);

  /**
   * Enters PlayMode for exactly one frame.
   * @param editor Editor instance.
   * @group editor.editor.play_mode
   */
  void ldk_editor_state_play_one_frame(LDKEditor *editor);

  /**
   * Loads a project
   * @param editor Editor instance.
   * @param project_path Path to the LDK project manifest.
   * @return True if the operation succeeds; false otherwise.
   * @group editor.editor.projects
   */
  bool ldk_editor_project_load(LDKEditor *editor, const char *project_path);

  /**
   * Quits the editor
   * @param editor Editor instance.
   * @group editor.editor.lifecycle
   */
  void ldk_editor_quit(LDKEditor *editor);


  //------------------------------------------------------------
  // Command System interface
  //------------------------------------------------------------

  /**
   * @brief Callback handling the arguments of an editor command.
   * @param args Args used by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group editor.editor.commands
   */
  typedef bool (*LDKEditorCommandFn)(XSlice args);
  /**
   * Registers an editor command
   * @param editor Editor instance.
   * @param cmd_name Name assigned to the editor command.
   * @param help Help text for the editor command.
   * @param fn Callback invoked by this operation.
   * @return True if the operation succeeds; false otherwise.
   * @group editor.editor.commands
   */
  bool ldk_editor_command_register(LDKEditor *editor, const char *cmd_name,
      const char *help, LDKEditorCommandFn fn);

  /**
   * Unregister an editor command
   * @param editor Editor instance.
   * @param cmd_name Name assigned to the editor command.
   * @return True if the operation succeeds; false otherwise.
   * @group editor.editor.commands
   */
  bool ldk_editor_command_unregister(LDKEditor *editor, const char *cmd_name);

  /**
   * Runs a command
   * @param editor Editor instance.
   * @param cmd_with_args Command-line text, including optional arguments.
   * @return True if the operation succeeds; false otherwise.
   * @group editor.editor.commands
   */
  bool ldk_editor_command_run(LDKEditor *editor, const char *cmd_with_args);

#ifdef __cplusplus
}
#endif

#endif // LDK_EDITOR_H
