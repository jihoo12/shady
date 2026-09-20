#ifndef SHADY_RENDER_H
#define SHADY_RENDER_H

#include <stdbool.h>

struct shady_output;
struct shady_server;
struct wlr_renderer;

bool shady_render_init(struct wlr_renderer *renderer);
void shady_render_fini(void);
void shady_render_output_frame(struct shady_output *output);
void shady_render_schedule_all_outputs(struct shady_server *server);

/* Shared with pick3d: build view/proj for the given output size. */
void shady_render_camera_matrices(struct shady_server *server,
	int buf_w, int buf_h, float view[16], float proj[16]);

#endif
