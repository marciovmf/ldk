#include <ldk_game.h>

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

