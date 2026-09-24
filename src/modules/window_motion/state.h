#ifndef SHADY_MODULE_WINDOW_MOTION_STATE_H
#define SHADY_MODULE_WINDOW_MOTION_STATE_H
#include <stdbool.h>
struct shady_window_motion_state {
	float wobble_x, wobble_y;
	float wobble_vx, wobble_vy;
	float tilt_x, tilt_y;
	float tilt_vx, tilt_vy;
	double last_move_x, last_move_y;
	bool wobble_dragging;
};
#endif
