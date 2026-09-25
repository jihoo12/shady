#ifndef SHADY_ENVIRONMENT_H
#define SHADY_ENVIRONMENT_H
#include <stdbool.h>
struct shady_server;
struct shady_gl_pipeline;
bool shady_environment_init(struct shady_gl_pipeline *pipeline);
bool shady_environment_load_colliders(struct shady_server *server);
void shady_environment_fini(void);
void shady_environment_draw(struct shady_server *server, struct shady_gl_pipeline *pipeline,
	const float view[16], const float proj[16]);
void shady_environment_draw_mesh(struct shady_server *server, const float vp[16]);
#endif
