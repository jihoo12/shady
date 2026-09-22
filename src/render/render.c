#include "render.h"

#include <time.h>
#include <math.h>
#include <stdlib.h>

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

struct shady_close_snapshot {
	struct wl_list link;

	struct shady_toplevel *toplevel;
	struct shady_server *server;

	GLuint texture;

	int texture_width;
	int texture_height;

	float x;
	float y;
	float width;
	float height;
	float tilt_x;
	float tilt_y;
	float z;
	float wobble_x;
	float wobble_y;

	bool has_alpha;

	bool dirty;
	bool animating;

	float progress;
};

static struct shady_close_snapshot *
find_close_snapshot(
	struct shady_toplevel *toplevel
);

static struct shady_close_snapshot *
ensure_close_snapshot(
	struct shady_toplevel *toplevel
);

static struct wl_list close_snapshots;

/*
 * Total close animation duration.
 *
 * 0.42 seconds feels quick enough for a window manager while still
 * making the effect clearly visible.
 */
#define CLOSE_ANIMATION_SECONDS 0.42f

/*
 * After sending the close request, give normally terminating clients
 * enough time to unmap before starting the reverse animation.
 */
#define CLOSE_WAIT_SECONDS 0.25f

/*
 * Slightly faster than the crumple animation.
 */
#define CLOSE_RESTORE_SECONDS 0.32f

#define FPS_FLOOR_Y -0.62f
#define FPS_EYE_HEIGHT 0.40f
#define FPS_MOVE_SPEED 1.25f
#define FPS_GRAVITY 3.8f
#define FPS_JUMP_SPEED 1.45f
#define WINDOW_GRAVITY 2.8f

static void update_fps_camera(struct shady_server *server, float dt) {
	struct shady_camera *cam = &server->camera;
	if (!cam->first_person) return;

	float sy = sinf(cam->yaw);
	float cy = cosf(cam->yaw);
	float fx = -sy, fz = -cy;
	float rx = cy, rz = -sy;
	float mx = 0.f, mz = 0.f;
	if (server->fps_forward) { mx += fx; mz += fz; }
	if (server->fps_back) { mx -= fx; mz -= fz; }
	if (server->fps_right) { mx += rx; mz += rz; }
	if (server->fps_left) { mx -= rx; mz -= rz; }
	float ml = sqrtf(mx * mx + mz * mz);
	if (ml > 0.001f) {
		cam->pos_x += mx / ml * FPS_MOVE_SPEED * dt;
		cam->pos_z += mz / ml * FPS_MOVE_SPEED * dt;
	}

	if (server->fps_jump_queued && cam->grounded) {
		cam->vel_y = FPS_JUMP_SPEED;
		cam->grounded = false;
	}
	server->fps_jump_queued = false;
	cam->vel_y -= FPS_GRAVITY * dt;
	cam->pos_y += cam->vel_y * dt;

	const float standing_y = FPS_FLOOR_Y + FPS_EYE_HEIGHT;
	if (cam->pos_y <= standing_y) {
		cam->pos_y = standing_y;
		cam->vel_y = 0.f;
		cam->grounded = true;
	}
}

static void update_window_gravity(
	struct shady_server *server, float dt, float logical_h
) {
	if (!server->window_gravity || dt <= 0.f || logical_h <= 0.f) return;

	const float FLOOR_RESTITUTION = 0.22f;
	const float FLOOR_FRICTION = 7.0f;
	const float ANGULAR_KICK = 0.22f;

	struct shady_toplevel *toplevel;
	wl_list_for_each(toplevel, &server->toplevels, link) {
		if (toplevel == server->fps_held_toplevel) {
			toplevel->physics_vy = 0.f;
			continue;
		}

		struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
		if (!surface->mapped) continue;

		float tw = (float)surface->current.width;
		float th = (float)surface->current.height;
		if (tw <= 0.f || th <= 0.f) continue;

		float world_w = tw / logical_h;
		float world_h = th / logical_h;
		float center_x = ((float)toplevel->scene_tree->node.x + tw * 0.5f -
			logical_h * 0.0f) / logical_h;
		float center_y = 0.5f -
			((float)toplevel->scene_tree->node.y + th * 0.5f) / logical_h;

		/*
		 * The lowest point of a rectangle rotated by tilt_x/tilt_y can be
		 * approximated from the projected half extents. Y rotation makes the
		 * horizontal span contribute to vertical support after the X rotation,
		 * which is enough to make tilted windows contact on an edge/corner
		 * instead of sinking to the old upright height.
		 */
		float sx = sinf(toplevel->tilt_x);
		float cx = cosf(toplevel->tilt_x);
		float sy = sinf(toplevel->tilt_y);
		float projected_half_h =
			fabsf(cx) * world_h * 0.5f +
			fabsf(sx * sy) * world_w * 0.5f;
		if (projected_half_h < 0.012f) projected_half_h = 0.012f;

		toplevel->physics_vy -= WINDOW_GRAVITY * dt;
		center_x += toplevel->physics_vx * dt;
		center_y += toplevel->physics_vy * dt;
		toplevel->z += toplevel->physics_vz * dt;

		float floor_center_y = FPS_FLOOR_Y + projected_half_h;
		if (center_y <= floor_center_y) {
			float impact = -toplevel->physics_vy;
			center_y = floor_center_y;

			if (impact > 0.12f) {
				/* A real landing bounces, twists, and excites the flexible body. */
				toplevel->physics_vy = impact * FLOOR_RESTITUTION;
				float side = (sinf(toplevel->tilt_y) >= 0.f) ? 1.f : -1.f;
				toplevel->tilt_vx += side * impact * ANGULAR_KICK;
				toplevel->tilt_vy -= sinf(toplevel->tilt_x) * impact * ANGULAR_KICK;
				toplevel->wobble_vx += side * impact * 0.018f;
				toplevel->wobble_vy += impact * 0.035f;
			} else {
				toplevel->physics_vy = 0.f;
			}

			/* Contact friction settles sliding and spin without erasing orientation. */
			float friction = 1.f - FLOOR_FRICTION * dt;
			if (friction < 0.f) friction = 0.f;
			toplevel->tilt_vx *= friction;
			toplevel->tilt_vy *= friction;
			toplevel->physics_vx *= friction;
			toplevel->physics_vz *= friction;
		}

		int x = (int)(center_x * logical_h - tw * 0.5f);
		int y = (int)((0.5f - center_y) * logical_h - th * 0.5f);
		wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
	}
}

static void update_fps_held_window(
	struct shady_server *server, float logical_w, float logical_h
) {
	struct shady_toplevel *toplevel = server->fps_held_toplevel;
	if (!server->camera.first_person || !toplevel) return;
	struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
	if (!surface->mapped || logical_h <= 0.f) {
		server->fps_held_toplevel = NULL;
		return;
	}

	struct shady_vec3 eye, forward;
	shady_camera_eye(&server->camera, &eye);
	shady_camera_basis(&server->camera, NULL, NULL, &forward);
	float d = server->fps_hold_distance;
	float cx = eye.x + forward.x * d;
	float cy = eye.y + forward.y * d;
	float cz = eye.z + forward.z * d;

	float tw = (float)surface->current.width;
	float th = (float)surface->current.height;
	float world_w = tw / logical_h;
	float world_h = th / logical_h;

	/* scene node is top-left in output pixels; held point is window center. */
	float left_world = cx - world_w * 0.5f;
	float bottom_world = cy - world_h * 0.5f;
	int x = (int)(left_world * logical_h + logical_w * 0.5f);
	int y = (int)(logical_h * 0.5f - (bottom_world + world_h) * logical_h);
	wlr_scene_node_set_position(&toplevel->scene_tree->node, x, y);
	toplevel->z = cz;

	/*
	 * Held windows are rigidly oriented toward the FPS camera.
	 *
	 * The regular animation loop aggressively springs tilt back toward zero,
	 * so feeding target values into tilt velocity made camera-facing rotation
	 * look like another wobble. Write the orientation directly here instead:
	 * this runs after the FPS camera update and before window animation/render.
	 *
	 * math3d applies Y rotation after X. The camera forward vector uses
	 * (-sin(yaw), sin(pitch), -cos(yaw)), so the panel's +Z normal faces back
	 * toward the camera with these angles.
	 */
	toplevel->tilt_x = -server->camera.pitch;
	toplevel->tilt_y = server->camera.yaw;
	toplevel->tilt_vx = 0.0f;
	toplevel->tilt_vy = 0.0f;

	/* Keep flexible wobble subtle while held so rigid rotation reads clearly. */
	toplevel->wobble_vx += forward.x * 0.00045f;
	toplevel->wobble_vy += forward.y * 0.00045f;
}


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
		 * Rigid-body tilt inertia.
		 *
		 * Rotation is now a persistent part of a window's 3D transform:
		 * releasing a window damps angular velocity, but does not spring the
		 * orientation back to the desktop XY plane.
		 */
		const float TILT_DAMPING = 9.0f;
		float tilt_damping = 1.0f - TILT_DAMPING * dt;
		if (tilt_damping < 0.0f) tilt_damping = 0.0f;
		toplevel->tilt_vx *= tilt_damping;
		toplevel->tilt_vy *= tilt_damping;
		toplevel->tilt_x += toplevel->tilt_vx * dt;
		toplevel->tilt_y += toplevel->tilt_vy * dt;

		/*
		* --------------------------------------------------------
		* Close animation state machine
		* --------------------------------------------------------
		*/

		switch (toplevel->close_state) {
		case SHADY_CLOSE_CRUMPLING:
			/*
			* Normal direction:
			*
			*     0.0 --------> 1.0
			*     normal       crumpled
			*/
			toplevel->close_progress +=
				dt /
				CLOSE_ANIMATION_SECONDS;

			if (
				toplevel->close_progress >=
				1.0f
			) {
				toplevel->close_progress =
					1.0f;

				toplevel->close_wait_time =
					0.0f;

				/*
				* Only now ask the client to close.
				*
				* If it immediately exits, xdg_toplevel_unmap()
				* will remove it before the restore animation
				* becomes visible.
				*/
				wlr_xdg_toplevel_send_close(
					toplevel->xdg_toplevel
				);

				toplevel->close_state =
					SHADY_CLOSE_WAITING;
			}

			break;

		case SHADY_CLOSE_WAITING:
			/*
			* The client has received the close request.
			*
			* A normal application will usually unmap during this
			* period. If it stays mapped, assume that it needs user
			* interaction and restore the window.
			*/
			toplevel->close_wait_time +=
				dt;

			if (
				toplevel->close_wait_time >=
				CLOSE_WAIT_SECONDS
			) {
				toplevel->close_state =
					SHADY_CLOSE_RESTORING;
			}

			break;

		case SHADY_CLOSE_RESTORING:
			/*
			* Reverse the exact same shader animation:
			*
			*     1.0 --------> 0.0
			*     crumpled     normal
			*/
			toplevel->close_progress -=
				dt /
				CLOSE_RESTORE_SECONDS;

			if (
				toplevel->close_progress <=
				0.0f
			) {
				toplevel->close_progress =
					0.0f;

				toplevel->close_wait_time =
					0.0f;

				toplevel->close_state =
					SHADY_CLOSE_ARMED;
			}

			break;
		case SHADY_CLOSE_ARMED:
			/*
			* The window is completely normal and interactive here.
			*
			* Later, the renderer will keep a compositor-owned snapshot
			* while this state is active. If the client actually unmaps,
			* that snapshot becomes the exit animation ghost.
			*/
			break;
		case SHADY_CLOSE_IDLE:
		default:
			break;
		}
	}
	struct shady_close_snapshot *snapshot;
	struct shady_close_snapshot *tmp;

	wl_list_for_each_safe(
		snapshot,
		tmp,
		&close_snapshots,
		link
	) {
		if (!snapshot->animating) {
			continue;
		}

		snapshot->progress +=
			dt /
			CLOSE_ANIMATION_SECONDS;

		if (
			snapshot->progress >=
			1.0f
		) {
			if (
				snapshot->texture
			) {
				glDeleteTextures(
					1,
					&snapshot->texture
				);
			}

			wl_list_remove(
				&snapshot->link
			);

			free(
				snapshot
			);
		}
	}
}

static struct shady_close_snapshot *
find_close_snapshot(
	struct shady_toplevel *toplevel
) {
	struct shady_close_snapshot *snapshot;

	wl_list_for_each(
		snapshot,
		&close_snapshots,
		link
	) {
		if (
			snapshot->toplevel ==
			toplevel
		) {
			return snapshot;
		}
	}

	return NULL;
}

static struct shady_close_snapshot *
ensure_close_snapshot(
	struct shady_toplevel *toplevel
) {
	struct shady_close_snapshot *snapshot =
		find_close_snapshot(
			toplevel
		);

	if (snapshot) {
		return snapshot;
	}

	snapshot =
		calloc(
			1,
			sizeof(*snapshot)
		);

	if (!snapshot) {
		return NULL;
	}

	snapshot->toplevel =
		toplevel;

	snapshot->server =
		toplevel->server;

	snapshot->dirty =
		true;

	wl_list_insert(
		&close_snapshots,
		&snapshot->link
	);

	return snapshot;
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

	wl_list_init(
		&close_snapshots
	);

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

	/*
	 * Draw a world-space reference plane before windows. Because it shares
	 * the depth buffer and camera VP matrix, orbiting immediately reveals
	 * perspective and per-window Z separation.
	 */
	shady_gl_pipeline_draw_floor(&pipeline, vp);

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

	/*
	 * The renderer already owns a monotonic frame clock. Reuse the same
	 * capped timestep as animation physics for first-person movement.
	 */
	static struct timespec fps_last_time;
	static bool fps_clock_ready;
	struct timespec fps_now;
	clock_gettime(CLOCK_MONOTONIC, &fps_now);
	float fps_dt = 0.f;
	if (fps_clock_ready) {
		fps_dt = (float)(fps_now.tv_sec - fps_last_time.tv_sec)
			+ (float)(fps_now.tv_nsec - fps_last_time.tv_nsec) / 1000000000.0f;
		if (fps_dt > 0.033f) fps_dt = 0.033f;
	}
	fps_last_time = fps_now;
	fps_clock_ready = true;
	update_fps_camera(server, fps_dt);
	update_window_gravity(server, fps_dt, logical_h);

	/* Camera physics changed the view, so rebuild matrices for this frame. */
	if (server->camera.first_person && fps_dt > 0.f) {
		shady_render_camera_matrices(server, buf_w, buf_h, view, proj);
		shady_mat4_multiply(vp, proj, view);
	}

	update_window_animations(
		server
	);

	/*
	 * Apply held-window pose after the generic tilt spring. This makes the
	 * camera-facing orientation authoritative for the current frame.
	 */
	update_fps_held_window(server, logical_w, logical_h);

	/*
	 * Project every mapped window onto the horizontal floor before drawing
	 * the windows themselves. The shadow shader uses the same deformation
	 * mesh and directional light as the window lighting.
	 */
	struct shady_toplevel *toplevel;
	wl_list_for_each_reverse(toplevel, &server->toplevels, link) {
		struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
		if (!surface->mapped || toplevel->close_progress >= 0.02f) {
			continue;
		}
		float tw = (float)surface->current.width;
		float th = (float)surface->current.height;
		if (tw <= 0.f || th <= 0.f) {
			continue;
		}
		float layout_x = (float)(toplevel->scene_tree->node.x + ox);
		float layout_y = (float)(toplevel->scene_tree->node.y + oy);
		float shadow_model[16];
		shady_window_model(shadow_model, layout_x, layout_y, tw, th,
			logical_w, logical_h, toplevel->z, toplevel->tilt_x, toplevel->tilt_y);
		shady_gl_pipeline_draw_shadow(&pipeline, vp, shadow_model,
			toplevel->wobble_x, toplevel->wobble_y, toplevel->z);
	}



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

		if (
			toplevel->close_state ==
			SHADY_CLOSE_ARMED
		) {
			struct shady_close_snapshot *snapshot =
				ensure_close_snapshot(
					toplevel
				);

			if (
				snapshot &&
				snapshot->dirty
			) {
				GLuint new_texture = 0;

				if (
					shady_gl_pipeline_copy_texture(
						&pipeline,
						attribs.target,
						attribs.tex,
						texture->width,
						texture->height,
						&new_texture
					)
				) {
					if (snapshot->texture) {
						glDeleteTextures(
							1,
							&snapshot->texture
						);
					}

					snapshot->texture = new_texture;
					snapshot->texture_width = texture->width;
					snapshot->texture_height = texture->height;
					snapshot->x =
						(float)toplevel->scene_tree->node.x;
					snapshot->y =
						(float)toplevel->scene_tree->node.y;
					snapshot->width = tw;
					snapshot->height = th;
					snapshot->tilt_x = toplevel->tilt_x;
					snapshot->tilt_y = toplevel->tilt_y;
					snapshot->z = toplevel->z;
					snapshot->wobble_x = toplevel->wobble_x;
					snapshot->wobble_y = toplevel->wobble_y;
					snapshot->has_alpha = attribs.has_alpha;
					snapshot->dirty = false;
				}
			}
		}

		shady_window_model(
			model,
			layout_x,
			layout_y,
			tw,
			th,
			logical_w,
			logical_h,
			toplevel->z,
			toplevel->tilt_x,
			toplevel->tilt_y
		);

		shady_mat4_multiply(
			mvp,
			vp,
			model
		);

		if (toplevel->close_progress < 0.02f) {
			shady_gl_pipeline_draw_sides(&pipeline, mvp, model, toplevel->wobble_x, toplevel->wobble_y);
		}

		shady_gl_pipeline_draw_window(
			&pipeline,
			attribs.target,
			attribs.tex,
			attribs.has_alpha,
			mvp,
			model,
			time_seconds,
			toplevel->wobble_x,
			toplevel->wobble_y,
			toplevel->close_progress
		);
	}

	struct shady_close_snapshot *snapshot;

	wl_list_for_each(
		snapshot,
		&close_snapshots,
		link
	) {
		if (
			!snapshot->animating ||
			!snapshot->texture ||
			snapshot->server != server
		) {
			continue;
		}

		float layout_x =
			snapshot->x +
			(float)ox;

		float layout_y =
			snapshot->y +
			(float)oy;

		float model[16];
		float mvp[16];

		shady_window_model(
			model,
			layout_x,
			layout_y,
			snapshot->width,
			snapshot->height,
			logical_w,
			logical_h,
			snapshot->z,
			snapshot->tilt_x,
			snapshot->tilt_y
		);

		shady_mat4_multiply(
			mvp,
			vp,
			model
		);

		if (snapshot->progress < 0.02f) {
			shady_gl_pipeline_draw_sides(&pipeline, mvp, model, snapshot->wobble_x, snapshot->wobble_y);
		}

		shady_gl_pipeline_draw_window(
			&pipeline,
			GL_TEXTURE_2D,
			snapshot->texture,
			snapshot->has_alpha,
			mvp,
			model,
			time_seconds,
			0.0f,
			0.0f,
			snapshot->progress
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



void shady_render_toplevel_commit(
	struct shady_toplevel *toplevel
) {
	if (
		toplevel->close_state !=
		SHADY_CLOSE_ARMED
	) {
		return;
	}

	struct shady_close_snapshot *snapshot =
		ensure_close_snapshot(
			toplevel
		);

	if (!snapshot) {
		return;
	}

	snapshot->dirty =
		true;

	shady_render_schedule_all_outputs(
		toplevel->server
	);
}

void shady_render_toplevel_unmap(
	struct shady_toplevel *toplevel
) {
	struct shady_close_snapshot *snapshot =
		find_close_snapshot(
			toplevel
		);

	if (
		!snapshot ||
		!snapshot->texture ||
		toplevel->close_state !=
			SHADY_CLOSE_ARMED
	) {
		return;
	}

	/*
	 * The client surface is going away.
	 *
	 * From this point onward the ghost owns everything it needs
	 * and must never dereference the toplevel again.
	 */
	snapshot->toplevel =
		NULL;

	snapshot->dirty =
		false;

	snapshot->animating =
		true;

	snapshot->progress =
		0.0f;

	shady_render_schedule_all_outputs(
		snapshot->server
	);
}

void shady_render_toplevel_destroy(
	struct shady_toplevel *toplevel
) {
	struct shady_close_snapshot *snapshot =
		find_close_snapshot(
			toplevel
	);

	if (!snapshot) {
		return;
	}

	/*
	 * If unmap already converted it into a ghost,
	 * find_close_snapshot() cannot find it because its toplevel
	 * pointer is NULL.
	 *
	 * Otherwise detach it so no dangling pointer remains.
	 */
	snapshot->toplevel =
		NULL;

	if (snapshot->texture) {
		snapshot->animating =
			true;

		snapshot->progress =
			0.0f;

		shady_render_schedule_all_outputs(
			snapshot->server
		);
	} else {
		wl_list_remove(
			&snapshot->link
		);

		free(
			snapshot
		);
	}
}