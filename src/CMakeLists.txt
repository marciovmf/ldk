project("ldk-core")
find_package(OpenGL REQUIRED)
set(INCLUDE_DIR "../include/ldk")

list(APPEND SOURCE
  ${INCLUDE_DIR}/ldk.h
  ${INCLUDE_DIR}/os.h
  ${INCLUDE_DIR}/gl.h
  gl.c
  ${INCLUDE_DIR}/maths.h
  maths.c
  ${INCLUDE_DIR}/arena.h
  arena.c
  ${INCLUDE_DIR}/engine.h
  engine.c
  ${INCLUDE_DIR}/common.h
  common.c
  ${INCLUDE_DIR}/module/graphics.h
  module/graphics.c
  ${INCLUDE_DIR}/eventqueue.h
  eventqueue.c
  )

if(WIN32)
  # Windows specific sources
  list(APPEND SOURCE win32_os.c)
endif()

if (LDK_AS_SHARED_LIB)
  add_library(ldk SHARED ${SOURCE})
  target_compile_definitions(ldk PRIVATE LDK_EXPORT_API LDK_AS_SHARED_LIB)
else()
  add_library(ldk STATIC ${SOURCE})
endif()

target_include_directories(ldk PUBLIC ${INCLUDE_DIR})
target_link_libraries(ldk PRIVATE ${LIBS} ${OPENGL_LIBRARY})
