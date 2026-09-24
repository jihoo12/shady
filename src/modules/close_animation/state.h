#ifndef SHADY_MODULE_CLOSE_ANIMATION_STATE_H
#define SHADY_MODULE_CLOSE_ANIMATION_STATE_H
enum shady_close_state {
	SHADY_CLOSE_IDLE,
	SHADY_CLOSE_CRUMPLING,
	SHADY_CLOSE_WAITING,
	SHADY_CLOSE_RESTORING,
	SHADY_CLOSE_ARMED,
};
struct shady_close_animation_state {
	enum shady_close_state state;
	float progress;
	float wait_time;
};
#endif
