#ifndef SHADY_RENDER_H
#define SHADY_RENDER_H

#include <stdbool.h>

struct shady_output;
struct wlr_renderer;

bool shady_render_init(struct wlr_renderer *renderer);
void shady_render_fini(void);
void shady_render_output_frame(struct shady_output *output);

#endif
