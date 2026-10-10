/* Engine-private accessors shared by independently compiled modules. */
#ifndef LDK_ENGINE_INTERNAL_H
#define LDK_ENGINE_INTERNAL_H

#include <ldk_game.h>

/* Valid after engine state construction, including before game.initialized. */
const LDKGame *ldki_engine_game_metadata_get(void);

#endif /* LDK_ENGINE_INTERNAL_H */
