#include "ldk_event.h"
#include "ldk_eventqueue.h"
#include <ldk_common.h>

#if defined(LDK_GAME)
#if defined(LDK_SHAREDLIB)
#define X_IMPL_ARRAY
#define X_IMPL_STRING
#define X_IMPL_FILESYSTEM
#define X_IMPL_LOG
#define X_IMPL_HASHTABLE
#define X_IMPL_HPOOL
#endif // LDK_SHAREDLIB
#endif // LDK_GAME

#define X_IMPL_FILESYSTEM
#include <ldk_game.h>
#include <ldk.h>

LDKGame game = {0};
const char* game_data = NULL;

bool on_window_event(const LDKEvent* event, void* state)
{
  if (event->windowEvent.type == LDK_WINDOW_EVENT_CLOSE)
  {
    printf("Closing game window\n");
    ldk_engine_stop(0);
    return true;
  }

  return false;
}

bool game_initialize(LDKGame* game)
{
  printf("Game initialize\n");
  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(q, on_window_event, LDK_EVENT_TYPE_WINDOW, NULL);
  return true;
}

bool game_start(LDKGame* game)
{
  printf("Game start\n");
  return true;
}

bool game_update(LDKGame* game, float delta_time)
{
  printf("%f\n", delta_time);
  return true;
}

bool game_terminate(LDKGame* game)
{
  printf("Game terminate\n");
  return true;
}

bool game_stop(LDKGame* game)
{
  printf("Game stop\n");
  return true;
}

i32 run_game(const char* ini_file_path)
{
  game.user_data = &game_data;
  game.initialize = game_initialize;
  game.start = game_start;
  game.update = game_update;
  game.stop = game_stop;
  game.terminate = game_terminate;

  if (!ldk_engine_initialize(&game, ini_file_path))
  {
    return 1;
  }

  ldk_engine_run();
  ldk_engine_terminate();
  return 0;
}

int main(i32 argc, char** argv)
{
  if (argc != 2)
    return 1;

  return run_game(argv[1]);
}

