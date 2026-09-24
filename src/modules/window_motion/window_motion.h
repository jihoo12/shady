#ifndef SHADY_MODULE_WINDOW_MOTION_H
#define SHADY_MODULE_WINDOW_MOTION_H
#include "../../shady.h"
#if SHADY_HAS_WINDOW_MOTION
void shady_window_motion_update_toplevel(struct shady_server *server, struct shady_toplevel *toplevel, float dt);
#else
static inline void shady_window_motion_update_toplevel(struct shady_server*s,struct shady_toplevel*t,float dt){(void)s;(void)t;(void)dt;}
#endif
#endif
