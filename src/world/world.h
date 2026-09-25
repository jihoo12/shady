#ifndef SHADY_WORLD_WORLD_H
#define SHADY_WORLD_WORLD_H
#include <stddef.h>
#include "collider.h"
#include "floor.h"
#include "platform.h"

#define SHADY_WORLD_MAX_COLLIDERS 32

struct shady_world {
	struct shady_box_collider colliders[SHADY_WORLD_MAX_COLLIDERS];
	size_t collider_count;
};

static inline bool shady_world_add_collider(struct shady_world *world,
		struct shady_box_collider collider) {
	if (world->collider_count >= SHADY_WORLD_MAX_COLLIDERS) return false;
	world->colliders[world->collider_count++] = collider;
	return true;
}

/* Build the current static world. OBJ/environment objects can register their
 * colliders here later without teaching physics about individual objects. */
static inline struct shady_world shady_world_default(void) {
	struct shady_world world = {0};
	struct shady_floor floor = shady_world_floor();
	shady_world_add_collider(&world, shady_floor_collider(&floor));
	shady_world_add_collider(&world, shady_world_test_platform());
	return world;
}

/* Find the highest supporting surface under an object's XZ footprint. */
static inline bool shady_world_support_y(const struct shady_world *world,
		const struct shady_box_collider *body, float *y_out) {
	bool found = false;
	float best = 0.0f;
	for (size_t i = 0; i < world->collider_count; ++i) {
		float y;
		if (!shady_box_support_y(&world->colliders[i], body, &y))
			continue;
		if (!found || y > best) {
			best = y;
			found = true;
		}
	}
	if (found && y_out) *y_out = best;
	return found;
}
#endif
