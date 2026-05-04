#include "module/ldk_component.h"
#include <ldk_event.h>
#include <module/ldk_eventqueue.h>
#include <module/ldk_ecs.h>
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
  if (event->window_event.type == LDK_WINDOW_EVENT_CLOSE)
  {
    ldk_log_info("Closing game window\n");
    ldk_engine_stop(0);
    return true;
  }

  return false;
}

#define HEALTH_COMPONENT_ID 1
typedef struct Health
{
  u32 hp;
  u32 hunger;
  u32 thirsty;
} Health;

bool game_initialize(LDKGame* game)
{
  ldk_log_info("Game initialize\n");

  LDKEventQueue *q = ldk_module_get(LDK_MODULE_EVENT);
  ldk_event_handler_add(q, on_window_event, LDK_EVENT_TYPE_WINDOW, NULL);

  // Register health component
  LDKComponentDesc desc = {0};
  desc.name = "Health";
  desc.type = HEALTH_COMPONENT_ID;
  ldk_ecs_component_register(&desc);

  LDKEntity e1 = ldk_ecs_entity_create();
  Health health = (Health){.hp = 100, .hunger = 0, .thirsty = 0 };
  ldk_ecs_component_add(e1, HEALTH_COMPONENT_ID, &health);

  return true;
}

bool game_start(LDKGame* game)
{
  ldk_log_info("Game start\n");
  return true;
}

void game_update(LDKGame* game, float delta_time)
{

}

void game_terminate(LDKGame* game)
{
  ldk_log_info("Game terminate\n");
}

void game_stop(LDKGame* game)
{
  ldk_log_info("Game stop\n");
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

