#include "ldk_editor_internal.h"
#include "ldk_os.h"
#include "ldk_project.h"
#include "stdx/stdx_strbuilder.h"
#include <stdx/stdx_array.h>
#include <stdx/stdx_string.h>
#include <string.h>

//------------------------------------------------------------
// Command System
//------------------------------------------------------------

static LDKEditorCommand *s_editor_command_find(
    XArray *cmd_array, XSlice cmd_name)
{
  u32 cmd_hash = x_slice_hash(cmd_name);
  u32 num_commands = x_array_count(cmd_array);
  for (u32 i = 0; i < num_commands; i++)
  {
    LDKEditorCommand *cmd = x_array_get(cmd_array, i);
    if (!cmd || cmd->hash != cmd_hash)
      continue;

    XSlice name = x_slice(cmd->name);
    if (x_slice_cmp(name, cmd_name) == 0)
      return cmd;
  }
  return NULL;
}

bool ldk_editor_command_register(LDKEditor *editor, const char *cmd_name,
    const char *cmd_help, LDKEditorCommandFn fn)
{
  LDKEditorContext *e = (LDKEditorContext *)editor;
  LDK_ASSERT(e != NULL);
  LDK_ASSERT(e->commands != NULL);
  LDK_ASSERT(cmd_name != NULL);

  LDKEditorCommand *cmd = s_editor_command_find(e->commands, x_slice(cmd_name));
  if (cmd)
    return false;

  const size_t cmd_name_len = strlen(cmd_name);
  const size_t cmd_help_len = strlen(cmd_help);
  LDK_ASSERT(cmd_name_len < X_SMALLSTR_MAX_LENGTH);
  LDK_ASSERT(cmd_help_len < X_SMALLSTR_MAX_LENGTH);

  LDKEditorCommand new_cmd = {0};
  new_cmd.cmd_func = fn;
  strncpy(&new_cmd.name[0], cmd_name, cmd_name_len);
  strncpy(new_cmd.help.buf, cmd_help, cmd_help_len);
  new_cmd.hash = x_cstr_hash(cmd_name);
  bool result = x_array_add(e->commands, &new_cmd) == 0;

  ldk_log_info("Registered command '%s'\n", cmd_name);
  return result;
}

bool ldk_editor_command_run(LDKEditor *editor, const char *cmd_with_args)
{
  LDKEditorContext *e = (LDKEditorContext *)editor;
  LDK_ASSERT(e != NULL);
  LDK_ASSERT(e->commands != NULL);

  XSlice input_slice = x_slice(cmd_with_args);
  XSlice cmd_slice = {0};
  XSlice args_slice = {0};
  if (!x_slice_split_at_white_space(input_slice, &cmd_slice, &args_slice))
  {
    cmd_slice = input_slice;
  }

  LDKEditorCommand *cmd = s_editor_command_find(e->commands, cmd_slice);
  if (!cmd)
  {
    ldki_editor_log_error(e, "Unknown command!\n");
    return false;
  }

  return cmd->cmd_func(args_slice);
}

//------------------------------------------------------------
// Native Commands
//------------------------------------------------------------

static bool s_editor_command_help(XSlice args)
{
  LDKEditorContext *e = ldk_editor_get();
  LDK_ASSERT(e != NULL);
  LDK_ASSERT(e->commands != NULL);

  XSlice cmd_slice = {0};
  XSlice args_slice = {0};
  if (!x_slice_split_at_white_space(args, &cmd_slice, &args_slice))
  {
    cmd_slice = args;
  }

  if (cmd_slice.length == 0)
  {
    XStrBuilder *sb = x_strbuilder_create();
    x_strbuilder_append_cstr(sb, "----------------------------\n");
    const u32 num_commands = x_array_count(e->commands);
    for (u32 i = 0; i < num_commands; i++)
    {
      LDKEditorCommand *cmd = (LDKEditorCommand *)x_array_get(e->commands, i);
      x_strbuilder_append_format(sb, "%s    %s\n", cmd->name, cmd->help);
    }
    ldki_editor_log_info(e, sb->data);
    x_strbuilder_destroy(sb);
    return true;
  }

  LDKEditorCommand *cmd = s_editor_command_find(e->commands, cmd_slice);
  if (!cmd)
  {
    ldki_editor_log_error(e, "Unknown command!\n");
    return false;
  }

  ldki_editor_log_info(e, cmd->help.buf);
  return true;
}

static bool s_editor_command_quit(XSlice args)
{
  ldk_editor_quit(ldk_editor_get());
  return true;
}

static bool s_editor_command_play(XSlice args)
{
  ldk_editor_state_set_play(ldk_editor_get());
  return true;
}

static bool s_editor_command_stop(XSlice args)
{
  ldk_editor_state_set_stop(ldk_editor_get());
  return true;
}

static bool s_editor_command_pause(XSlice args)
{
  ldk_editor_state_set_pause(ldk_editor_get());
  return true;
}

static bool s_editor_command_step(XSlice args)
{
  ldk_editor_state_play_one_frame(ldk_editor_get());
  return true;
}

static bool s_editor_command_project(XSlice args)
{
  XSlice path_arg;
  LDKEditorContext *editor = (LDKEditorContext *)ldk_editor_get();

  if (!x_slice_next_token_white_space(&args, &path_arg))
  {
    return ldki_editor_show_open_project_dialog(editor, NULL);
  }

  XSmallstr path = {0};
  x_smallstr_from_slice(path_arg, &path);
  return ldki_editor_project_open_request(editor, path.buf);
}

static bool s_editor_command_projnew(XSlice args)
{
  (void)args;
  return ldki_editor_project_create_window_open(
      (LDKEditorContext *)ldk_editor_get());
}

static bool s_editor_command_projbuild(XSlice args)
{
  LDKEditorContext *editor = (LDKEditorContext *)ldk_editor_get();
  (void)args;

  if (!editor->project.loaded)
  {
    ldki_editor_log_error(editor, "No project is loaded.");
    return false;
  }

  if (!ldki_editor_project_build_request(editor))
  {
    ldki_editor_log_error(editor, "A project action is already pending.");
    return false;
  }

  return true;
}

static bool s_editor_command_projrelease(XSlice args)
{
  LDKEditorContext *editor = (LDKEditorContext *)ldk_editor_get();
  (void)args;

  if (!editor->project.loaded)
  {
    ldki_editor_log_error(editor, "No project is loaded.");
    return false;
  }

  if (!ldki_editor_project_release_request(editor))
  {
    ldki_editor_log_error(editor, "A project action is already pending.");
    return false;
  }

  return true;
}

static bool s_editor_command_layout_save_as(XSlice args)
{
  XSlice arg0 = {0};
  if (!x_slice_next_token_white_space(&args, &arg0))
  {
    LDKEditorContext *e = ldk_editor_get();
    ldki_editor_log_error(e, "Command requires layout name argument!\n");
    return false;
  }

  XSmallstr name = {0};
  x_smallstr_from_slice(arg0, &name);
  ldki_editor_dock_layout_create(name.buf);
  return true;
}

static bool s_editor_command_layout_list(XSlice args)
{
  LDKEditorContext *e = ldk_editor_get();
  u32 layout_count = ldki_editor_dock_layout_count();
  for (u32 i = 0; i < layout_count; ++i)
  {
    const char *layout_name = ldki_editor_dock_layout_name_get(i);

    if (layout_name != NULL)
    {
      ldki_editor_log_info(e, layout_name);
    }
  }
  return true;
}

static bool s_editor_command_layout_set(XSlice args)
{
  XSlice arg0 = {0};
  if (!x_slice_next_token_white_space(&args, &arg0))
  {
    LDKEditorContext *e = ldk_editor_get();
    ldki_editor_log_error(e, "Command requires layout name argument!\n");
    return false;
  }

  XSmallstr name = {0};
  x_smallstr_from_slice(arg0, &name);
  return ldki_editor_dock_layout_load(name.buf);
}

static bool s_editor_command_layout_delete(XSlice args)
{
  LDKEditorContext *editor = ldk_editor_get();
  XStrBuilder *out;
  XSmallstr name = {0};
  bool saved;

  args = x_slice_trim(args);
  if (args.length == 0)
  {
    ldki_editor_log_error(editor, "Command requires layout name argument!");
    return false;
  }

  if (x_smallstr_from_slice(args, &name) == 0)
  {
    ldki_editor_log_error(editor, "Invalid layout name!");
    return false;
  }

  out = x_strbuilder_create();
  if (out == NULL)
  {
    ldki_editor_log_error(editor, "Failed to allocate dock layout output!");
    return false;
  }

  if (!ldki_editor_dock_layout_delete(name.buf))
  {
    x_strbuilder_destroy(out);

    if (strcmp(name.buf, "default") == 0)
    {
      ldki_editor_log_error(editor, "The default layout cannot be deleted!");
    }
    else
    {
      ldki_editor_log_error(editor, "Failed to delete layout!");
    }

    return false;
  }

  saved = ldki_editor_dock_layout_save(out);
  x_strbuilder_destroy(out);

  if (!saved)
  {
    ldki_editor_log_error(
        editor, "Layout deleted, but failed to save the layout file!");
    return false;
  }

  ldki_editor_log_info(editor, "Layout deleted.");
  return true;
}

//------------------------------------------------------------
// Internal
//------------------------------------------------------------
void ldki_editor_register_commands(LDKEditorContext *editor)
{
  LDK_ASSERT(editor != NULL);
  LDK_ASSERT(editor->commands == NULL);
  editor->commands = x_array_create(
      sizeof(LDKEditorCommand), LDK_EDITOR_COMMAND_INITIAL_CAPACITY);

  ldk_editor_command_register(editor, "help",
      "Shows help information for a given command.", s_editor_command_help);

  ldk_editor_command_register(
      editor, "projnew", "Creates a new project.", s_editor_command_projnew);
  ldk_editor_command_register(editor, "projbuild",
      "Builds the current project game module.", s_editor_command_projbuild);
  ldk_editor_command_register(editor, "projrelease",
      "Builds the current project launcher in Release configuration.",
      s_editor_command_projrelease);
  ldk_editor_command_register(
      editor, "play", "Enter Play mode.", s_editor_command_play);
  ldk_editor_command_register(
      editor, "stop", "Leaves Play mode.", s_editor_command_stop);
  ldk_editor_command_register(
      editor, "pause", "Pauses Play mode.", s_editor_command_pause);
  ldk_editor_command_register(editor, "step",
      "Advances Play mode by one frame.", s_editor_command_step);
  ldk_editor_command_register(
      editor, "project", "Loads a project.", s_editor_command_project);
  ldk_editor_command_register(
      editor, "quit", "Terminates the editor.", s_editor_command_quit);

  ldk_editor_command_register(editor, "layout-save-as",
      "Saves the current layout as a new name.",
      s_editor_command_layout_save_as);

  ldk_editor_command_register(editor, "layout-list", "List all layout names.",
      s_editor_command_layout_list);

  ldk_editor_command_register(editor, "layout-set",
      "Set the current layout as the one identified by name.",
      s_editor_command_layout_set);

  ldk_editor_command_register(editor, "layout-delete",
      "Deletes the layout identified by name.", s_editor_command_layout_delete);
}
