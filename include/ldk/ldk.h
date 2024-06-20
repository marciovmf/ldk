/**
 *
 * ldk.h
 * 
 * This is the main public include for projects using this engine.
 *
 */

#ifndef LDK_H
#define LDK_H

/* Core */
#include <ldk/common.h>
#include <ldk/os.h>
#include <ldk/gl.h>
#include <ldk/hlist.h>
#include <ldk/arena.h>
#include <ldk/array.h>
#include <ldk/maths.h>
#include <ldk/engine.h>
#include <ldk/eventqueue.h>

/* Modules */
#include <ldk/module/graphics.h>
#include <ldk/module/entity.h>
#include <ldk/module/renderer.h>
#include <ldk/module/asset.h>

/* Assets */
#include <ldk/asset/shader.h>
#include <ldk/asset/material.h>
#include <ldk/asset/mesh.h>
#include <ldk/asset/image.h>
#include <ldk/asset/texture.h>
#include <ldk/asset/config.h>

/* Entities */
#include <ldk/entity/camera.h>
#include <ldk/entity/staticobject.h>
#include <ldk/entity/instancedobject.h>
#include <ldk/entity/pointlight.h>
#include <ldk/entity/directionallight.h>
#include <ldk/entity/spotlight.h>


#endif //LDK_H
