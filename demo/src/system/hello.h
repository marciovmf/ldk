#ifndef DEMO_HELLO_H
#define DEMO_HELLO_H

#include <module/ldk_system.h>

//@system update=s_hello_system_update
typedef struct Hello
{
  u32 log_every_n_updates;

  //@inspect runtime
  u32 update_count;
} Hello;

#endif
