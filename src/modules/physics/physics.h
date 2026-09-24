#ifndef SHADY_MODULE_PHYSICS_H
#define SHADY_MODULE_PHYSICS_H
#include "../../shady.h"
#if SHADY_HAS_PHYSICS
void shady_physics_init(struct shady_server *server);
void shady_physics_toggle_gravity(struct shady_server *server);
void shady_physics_update(struct shady_server *server, float dt, float logical_w, float logical_h);
#else
static inline void shady_physics_init(struct shady_server *server) { server->window_gravity=false; }
static inline void shady_physics_toggle_gravity(struct shady_server *server) { (void)server; }
static inline void shady_physics_update(struct shady_server *server,float dt,float w,float h){(void)server;(void)dt;(void)w;(void)h;}
#endif
#endif
