#ifndef SHADY_MODULE_PHYSICS_H
#define SHADY_MODULE_PHYSICS_H
#include "../../shady.h"

struct shady_window_body {
	float center[3];
	float half[3];
	float tilt_x, tilt_y;
};
#if SHADY_HAS_PHYSICS
void shady_physics_init(struct shady_server *server);
void shady_physics_toggle_gravity(struct shady_server *server);
void shady_physics_update(struct shady_server *server, float dt, float logical_w, float logical_h);
void shady_physics_set_velocity(struct shady_toplevel *toplevel, float vx, float vy, float vz);
void shady_physics_stop(struct shady_toplevel *toplevel);
bool shady_physics_window_body(const struct shady_toplevel *toplevel,
	float logical_w, float logical_h, struct shady_window_body *body);
#else
static inline void shady_physics_set_velocity(struct shady_toplevel*t,float x,float y,float z){(void)t;(void)x;(void)y;(void)z;}
static inline void shady_physics_stop(struct shady_toplevel*t){(void)t;}
static inline void shady_physics_init(struct shady_server *server) { server->physics.gravity_enabled=false; }
static inline void shady_physics_toggle_gravity(struct shady_server *server) { (void)server; }
static inline void shady_physics_update(struct shady_server *server,float dt,float w,float h){(void)server;(void)dt;(void)w;(void)h;}
#endif
#endif
