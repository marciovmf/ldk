list(APPEND LDK_GAME_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/demo.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/component/player_character.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/component/tftf_bulb_plant_ai.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/component/tftf_projectile.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/system/island_terrain.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/system/player_character_controller.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/system/tftf_bulb_plant_ai_system.c"
  "${CMAKE_CURRENT_LIST_DIR}/src/system/tftf_bullet_system.c"
)
list(APPEND LDK_GAME_INCLUDE_DIRS "")
list(APPEND LDK_GAME_DEFINITIONS "")
list(APPEND LDK_GAME_LIBRARIES "")
list(APPEND LDK_GAME_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/component")
list(APPEND LDK_GAME_SYSTEM_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/system")
