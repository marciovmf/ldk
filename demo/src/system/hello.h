#ifndef DEMO_HELLO_H
#define DEMO_HELLO_H

#include <module/ldk_system.h>

void hello_system_update(void *data, const LDKEntityGroup *group, float dt);

//@system update=hello_system_update
typedef struct Hello
{
  u32 log_every_n_updates;

  //@inspect runtime
  u32 update_count;
} Hello;

#endif

