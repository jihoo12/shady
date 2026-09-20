#include "render.h"

#include <time.h>
#include <wayland-server-core.h>
#include <wlr/render/gles2.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <GLES2/gl2.h>

#include "../shady.h"
#include "gl_pipeline.h"

static struct shady_gl_pipeline pipeline;
static bool pipeline_ready;

bool shady_render_init(struct wlr_renderer *renderer) {
	pipeline_ready = shady_gl_pipeline_init(&pipeline, renderer);
	return pipeline_ready;
}

void shady_render_fini(void) {
	if (pipeline_ready) {
		shady_gl_pipeline_fini(&pipeline);
		pipeline_ready = false;
	}
}

static void send_frame_done_surface(struct wlr_surface *surface,
		int sx, int sy, void *data) {
	(void)sx;
	(void)sy;
	wlr_surface_send_frame_done(surface, data);
}

void shady_render_output_frame(struct shady_output *output) {
	struct shady_server *server = output->server;
	struct wlr_output *wlr_output = output->wlr_output;

	if (!pipeline_ready) {
		return;
	}

	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(server->scene, wlr_output);
	if (!scene_output) {
		return;
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);

	struct wlr_render_pass *pass =
		wlr_output_begin_render_pass(wlr_output, &state, NULL);
	if (!pass) {
		wlr_output_state_finish(&state);
		return;
	}

	/* Buffer pixel size (matches wlroots begin_gles2_buffer_pass viewport). */
	int buf_w = wlr_output->width;
	int buf_h = wlr_output->height;
	float scale = wlr_output->scale;

	glViewport(0, 0, buf_w, buf_h);
	glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);

	double ox = 0, oy = 0;
	wlr_output_layout_output_coords(server->output_layout, wlr_output, &ox, &oy);

	/* Bottom-to-top so the focused (list head) window is drawn last. */
	struct shady_toplevel *toplevel;
	wl_list_for_each_reverse(toplevel, &server->toplevels, link) {
		struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
		if (!surface->mapped) {
			continue;
		}

		struct wlr_texture *texture = wlr_surface_get_texture(surface);
		if (!texture || !wlr_texture_is_gles2(texture)) {
			continue;
		}

		struct wlr_gles2_texture_attribs attribs;
		wlr_gles2_texture_get_attribs(texture, &attribs);

		/* Layout coords → buffer pixels (wlroots render-pass space). */
		float x = (float)((toplevel->scene_tree->node.x + ox) * scale);
		float y = (float)((toplevel->scene_tree->node.y + oy) * scale);
		float tw = (float)surface->current.width * scale;
		float th = (float)surface->current.height * scale;
		if (tw <= 0.f || th <= 0.f) {
			tw = (float)texture->width;
			th = (float)texture->height;
		}

		shady_gl_pipeline_draw_window(&pipeline, attribs.target, attribs.tex,
			attribs.has_alpha, x, y, tw, th, buf_w, buf_h);
	}

	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "failed to submit render pass");
	}
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	/*
	 * Do not rely on wlr_scene_output_send_frame_done alone: without a scene
	 * commit, visibility / pacing can skip callbacks and clients (e.g. foot)
	 * never redraw past an empty/black buffer.
	 */
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wl_list_for_each(toplevel, &server->toplevels, link) {
		struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
		if (!surface->mapped) {
			continue;
		}
		wlr_surface_for_each_surface(surface, send_frame_done_surface, &now);
	}
	(void)scene_output;
}
