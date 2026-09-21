#include "render.h"

#include <time.h>
#include <math.h>

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
#include "math3d.h"

static struct shady_gl_pipeline pipeline;
static bool pipeline_ready;

static GLuint depth_rbo;
static int depth_rbo_w;
static int depth_rbo_h;

/*
 * Monotonic starting point for shader animation.
 */
static struct timespec shader_start_time;
static struct timespec wobble_last_time;

/*
 * Total close animation duration.
 *
 * 0.42 seconds feels quick enough for a window manager while still
 * making the effect clearly visible.
 */
#define CLOSE_ANIMATION_SECONDS 0.42f

static float shader_time_seconds(void) {
	struct timespec now;

	clock_gettime(
		CLOCK_MONOTONIC,
		&now
	);

	double seconds =
		(double)(
			now.tv_sec -
			shader_start_time.tv_sec
		);

	double nanoseconds =
		(double)(
			now.tv_nsec -
			shader_start_time.tv_nsec
		) / 1000000000.0;

	return (float)(
		seconds + nanoseconds
	);
}

static void update_window_animations(
	struct shady_server *server
) {
	struct timespec now;

	clock_gettime(
		CLOCK_MONOTONIC,
		&now
	);

	float dt =
		(float)(
			now.tv_sec -
			wobble_last_time.tv_sec
		) +
		(float)(
			now.tv_nsec -
			wobble_last_time.tv_nsec
		) / 1000000000.0f;

	wobble_last_time =
		now;

	if (dt <= 0.0f) {
		return;
	}

	if (dt > 0.033f) {
		dt = 0.033f;
	}

	const float SPRING =
		42.0f;

	const float DAMPING =
		7.5f;

	struct shady_toplevel *toplevel;

	wl_list_for_each(
		toplevel,
		&server->toplevels,
		link
	) {
		/*
		 * --------------------------------------------------------
		 * Existing wobble simulation
		 * --------------------------------------------------------
		 */

		float ax =
			-toplevel->wobble_x *
			SPRING;

		float ay =
			-toplevel->wobble_y *
			SPRING;

		toplevel->wobble_vx +=
			ax *
			dt;

		toplevel->wobble_vy +=
			ay *
			dt;

		float damping =
			1.0f -
			DAMPING *
			dt;

		if (damping < 0.0f) {
			damping = 0.0f;
		}

		toplevel->wobble_vx *=
			damping;

		toplevel->wobble_vy *=
			damping;

		toplevel->wobble_x +=
			toplevel->wobble_vx *
			dt;

		toplevel->wobble_y +=
			toplevel->wobble_vy *
			dt;

		if (
			fabsf(toplevel->wobble_x) <
				0.00005f &&
			fabsf(toplevel->wobble_vx) <
				0.00005f
		) {
			toplevel->wobble_x =
				0.0f;

			toplevel->wobble_vx =
				0.0f;
		}

		if (
			fabsf(toplevel->wobble_y) <
				0.00005f &&
			fabsf(toplevel->wobble_vy) <
				0.00005f
		) {
			toplevel->wobble_y =
				0.0f;

			toplevel->wobble_vy =
				0.0f;
		}

		/*
		 * --------------------------------------------------------
		 * Close animation
		 * --------------------------------------------------------
		 */

		if (
			toplevel->closing &&
			!toplevel->close_sent
		) {
			toplevel->close_progress +=
				dt /
				CLOSE_ANIMATION_SECONDS;

			if (
				toplevel->close_progress >=
				1.0f
			) {
				toplevel->close_progress =
					1.0f;

				/*
				 * Mark first.
				 *
				 * The client may react immediately and cause
				 * lifecycle callbacks, so don't send this twice.
				 */
				toplevel->close_sent =
					true;

				wlr_xdg_toplevel_send_close(
					toplevel->xdg_toplevel
				);
			}
		}
	}
}

bool shady_render_init(
	struct wlr_renderer *renderer
) {
	pipeline_ready =
		shady_gl_pipeline_init(
			&pipeline,
			renderer
		);

	depth_rbo = 0;
	depth_rbo_w = 0;
	depth_rbo_h = 0;

	clock_gettime(
		CLOCK_MONOTONIC,
		&shader_start_time
	);

	wobble_last_time =
		shader_start_time;

	return pipeline_ready;
}

void shady_render_fini(void) {
	if (depth_rbo) {
		glDeleteRenderbuffers(
			1,
			&depth_rbo
		);

		depth_rbo = 0;
	}

	if (pipeline_ready) {
		shady_gl_pipeline_fini(
			&pipeline
		);

		pipeline_ready = false;
	}
}

void shady_render_camera_matrices(
	struct shady_server *server,
	int buf_w,
	int buf_h,
	float view[16],
	float proj[16]
) {
	shady_camera_view(
		&server->camera,
		view
	);

	float aspect =
		(buf_h > 0)
			? ((float)buf_w / (float)buf_h)
			: 1.f;

	shady_mat4_perspective(
		proj,
		SHADY_CAMERA_FOV_Y,
		aspect,
		SHADY_CAMERA_NEAR,
		SHADY_CAMERA_FAR
	);
}

void shady_render_schedule_all_outputs(
	struct shady_server *server
) {
	struct shady_output *output;

	wl_list_for_each(
		output,
		&server->outputs,
		link
	) {
		wlr_output_schedule_frame(
			output->wlr_output
		);
	}
}

static void ensure_depth_rbo(
	int w,
	int h
) {
	if (
		depth_rbo &&
		depth_rbo_w == w &&
		depth_rbo_h == h
	) {
		return;
	}

	if (depth_rbo) {
		glDeleteRenderbuffers(
			1,
			&depth_rbo
		);

		depth_rbo = 0;
	}

	glGenRenderbuffers(
		1,
		&depth_rbo
	);

	glBindRenderbuffer(
		GL_RENDERBUFFER,
		depth_rbo
	);

	glRenderbufferStorage(
		GL_RENDERBUFFER,
		GL_DEPTH_COMPONENT16,
		w,
		h
	);

	glBindRenderbuffer(
		GL_RENDERBUFFER,
		0
	);

	depth_rbo_w = w;
	depth_rbo_h = h;
}

static void send_frame_done_surface(
	struct wlr_surface *surface,
	int sx,
	int sy,
	void *data
) {
	(void)sx;
	(void)sy;

	wlr_surface_send_frame_done(
		surface,
		data
	);
}

void shady_render_output_frame(
	struct shady_output *output
) {
	struct shady_server *server =
		output->server;

	struct wlr_output *wlr_output =
		output->wlr_output;

	if (!pipeline_ready) {
		return;
	}

	struct wlr_scene_output *scene_output =
		wlr_scene_get_scene_output(
			server->scene,
			wlr_output
		);

	if (!scene_output) {
		return;
	}

	struct wlr_output_state state;

	wlr_output_state_init(
		&state
	);

	struct wlr_render_pass *pass =
		wlr_output_begin_render_pass(
			wlr_output,
			&state,
			NULL
		);

	if (!pass) {
		wlr_output_state_finish(
			&state
		);

		return;
	}

	int buf_w =
		wlr_output->width;

	int buf_h =
		wlr_output->height;

	float scale =
		wlr_output->scale;

	float logical_w =
		(float)buf_w / scale;

	float logical_h =
		(float)buf_h / scale;

	/*
	 * Attach a depth RBO to wlroots'
	 * color-only FBO for 3D occlusion.
	 */
	GLint fbo = 0;

	glGetIntegerv(
		GL_FRAMEBUFFER_BINDING,
		&fbo
	);

	ensure_depth_rbo(
		buf_w,
		buf_h
	);

	glBindFramebuffer(
		GL_FRAMEBUFFER,
		(GLuint)fbo
	);

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER,
		depth_rbo
	);

	glViewport(
		0,
		0,
		buf_w,
		buf_h
	);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	/*
	 * Slightly darker background makes
	 * the neon edges more visible.
	 */
	glClearColor(
		0.055f,
		0.060f,
		0.085f,
		1.0f
	);

	glClearDepthf(1.0f);

	glClear(
		GL_COLOR_BUFFER_BIT |
		GL_DEPTH_BUFFER_BIT
	);

	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);

	float view[16];
	float proj[16];
	float vp[16];

	shady_render_camera_matrices(
		server,
		buf_w,
		buf_h,
		view,
		proj
	);

	shady_mat4_multiply(
		vp,
		proj,
		view
	);

	double ox = 0;
	double oy = 0;

	wlr_output_layout_output_coords(
		server->output_layout,
		wlr_output,
		&ox,
		&oy
	);

	/*
	 * Same time value for every window
	 * rendered in this frame.
	 */
	float time_seconds =
		shader_time_seconds();

	update_window_animations(
		server
	);

	struct shady_toplevel *toplevel;

	wl_list_for_each_reverse(
		toplevel,
		&server->toplevels,
		link
	) {
		struct wlr_surface *surface =
			toplevel
				->xdg_toplevel
				->base
				->surface;

		if (!surface->mapped) {
			continue;
		}

		struct wlr_texture *texture =
			wlr_surface_get_texture(
				surface
			);

		if (
			!texture ||
			!wlr_texture_is_gles2(texture)
		) {
			continue;
		}

		struct wlr_gles2_texture_attribs attribs;

		wlr_gles2_texture_get_attribs(
			texture,
			&attribs
		);

		float tw =
			(float)surface->current.width;

		float th =
			(float)surface->current.height;

		if (tw <= 0.f || th <= 0.f) {
			tw =
				(float)texture->width /
				scale;

			th =
				(float)texture->height /
				scale;
		}

		float layout_x =
			(float)(
				toplevel
					->scene_tree
					->node
					.x +
				ox
			);

		float layout_y =
			(float)(
				toplevel
					->scene_tree
					->node
					.y +
				oy
			);

		float model[16];
		float mvp[16];

		shady_window_model(
			model,
			layout_x,
			layout_y,
			tw,
			th,
			logical_w,
			logical_h
		);

		shady_mat4_multiply(
			mvp,
			vp,
			model
		);

		shady_gl_pipeline_draw_window(
			&pipeline,
			attribs.target,
			attribs.tex,
			attribs.has_alpha,
			mvp,
			time_seconds,
			toplevel->wobble_x,
			toplevel->wobble_y,
			toplevel->close_progress
		);
	}

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER,
		0
	);

	glDisable(GL_DEPTH_TEST);

	if (!wlr_render_pass_submit(pass)) {
		wlr_log(
			WLR_ERROR,
			"failed to submit render pass"
		);
	}

	wlr_output_commit_state(
		wlr_output,
		&state
	);

	wlr_output_state_finish(
		&state
	);

	struct timespec now;

	clock_gettime(
		CLOCK_MONOTONIC,
		&now
	);

	wl_list_for_each(
		toplevel,
		&server->toplevels,
		link
	) {
		struct wlr_surface *surface =
			toplevel
				->xdg_toplevel
				->base
				->surface;

		if (!surface->mapped) {
			continue;
		}

		wlr_surface_for_each_surface(
			surface,
			send_frame_done_surface,
			&now
		);
	}

	(void)scene_output;

	/*
	 * u_time changes continuously, so request
	 * another frame even when clients are idle.
	 */
	wlr_output_schedule_frame(
		wlr_output
	);
}