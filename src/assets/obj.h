#ifndef SHADY_ASSETS_OBJ_H
#define SHADY_ASSETS_OBJ_H

#include <stdbool.h>
#include "mesh.h"
#include "../world/collider.h"

/* Load a Wavefront OBJ into Shady's CPU-side triangle mesh.
 * Supported faces: v, v/vt, v//vn, v/vt/vn. Polygons are triangulated as a
 * fan and both positive and negative OBJ indices are accepted. */
bool shady_obj_load(const char *path, struct shady_mesh *mesh);

/* Extract axis-aligned collision boxes from OBJ groups whose names start with
 * "collision_". Face vertices contribute to that group's bounds. */
bool shady_obj_load_colliders(const char *path,
	struct shady_box_collider *colliders, size_t capacity, size_t *count);

#endif
