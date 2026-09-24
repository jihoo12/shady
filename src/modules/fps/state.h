#ifndef SHADY_MODULE_FPS_STATE_H
#define SHADY_MODULE_FPS_STATE_H
#include <stdbool.h>
struct shady_toplevel;
struct shady_fps_state {
	bool forward, back, left, right;
	bool jump_queued;
	struct shady_toplevel *held_toplevel;
	float hold_distance;
	/* World-space offset from window center to the point actually grabbed. */
	float grab_offset_x, grab_offset_y, grab_offset_z;
	bool input_capture;
};
#endif
