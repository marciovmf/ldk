#include <ldk_game.h>
#include <module/ldk_scene_manager.h>

static i32 s_run_game(const char* ini_file_path)
{
  if (!ldk_engine_initialize(ini_file_path))
  {
    return 1;
  }

  if (!ldk_game_instance_load_static())
  {
    ldk_engine_terminate();
    return 1;
  }

  if (!ldk_game_instance_initialize())
  {
    ldk_engine_terminate();
    return 1;
  }

  LDKSceneManager *manager = ldk_module_get(LDK_MODULE_SCENE_MANAGER);
  LDKSceneResult result;
  const LDKConfig *config = ldk_engine_config_get();
  if (!ldk_scene_manager_configure_file(
          manager, ini_file_path, &config->runtree_path, &result) ||
      !ldk_scene_manager_load(manager, 0, &result))
  {
    ldk_log_error("Failed to load initial scene: %s\n", result.error);
    ldk_engine_terminate();
    return 1;
  }

  if (!ldk_game_instance_start())
  {
    ldk_engine_terminate();
    return 1;
  }

  u32 exit_code = ldk_engine_run();

  ldk_engine_terminate();
  return exit_code;
}

int main(i32 argc, char** argv)
{
  if (argc != 2)
  {
    printf("Usage:\n%s ini_file\n", argv[0]);
    return 1;
  }

  char* ini_file_path = argv[1];
  return s_run_game(ini_file_path);
}
