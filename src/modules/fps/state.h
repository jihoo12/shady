#ifndef SHADY_MODULE_FPS_STATE_H
#define SHADY_MODULE_FPS_STATE_H
#include <stdbool.h>
struct shady_toplevel;
struct wl_pointer;
struct wl_registry;
struct zwp_pointer_constraints_v1;
struct zwp_locked_pointer_v1;
struct shady_fps_state {
	bool forward, back, left, right;
	bool jump_queued;
	struct shady_toplevel *held_toplevel;
	float hold_distance;
	bool input_capture;
	struct wl_registry *host_registry;
	struct wl_pointer *host_pointer;
	struct zwp_pointer_constraints_v1 *host_constraints;
	struct zwp_locked_pointer_v1 *host_lock;
};
#endif
