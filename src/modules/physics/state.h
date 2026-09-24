#ifndef SHADY_MODULE_PHYSICS_STATE_H
#define SHADY_MODULE_PHYSICS_STATE_H
#include <stdbool.h>
struct shady_physics_state { bool gravity_enabled; };
struct shady_window_physics_state { float z; float vx, vy, vz; };
#endif
