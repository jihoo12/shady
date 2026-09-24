#ifndef SHADY_MODULE_WINDOW_MOTION_H
#define SHADY_MODULE_WINDOW_MOTION_H
#include "../../shady.h"
#if SHADY_HAS_WINDOW_MOTION
void shady_window_motion_update_toplevel(struct shady_server *server, struct shady_toplevel *toplevel, float dt);
void shady_window_motion_add_impulse(struct shady_server *server, struct shady_toplevel *toplevel,
	float wobble_x, float wobble_y, float tilt_x, float tilt_y);
#else
static inline void shady_window_motion_add_impulse(struct shady_server*s,struct shady_toplevel*t,float wx,float wy,float tx,float ty){(void)s;(void)t;(void)wx;(void)wy;(void)tx;(void)ty;}
static inline void shady_window_motion_update_toplevel(struct shady_server*s,struct shady_toplevel*t,float dt){(void)s;(void)t;(void)dt;}
#endif
#endif
