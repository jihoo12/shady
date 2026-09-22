/* Adapted from wlroots 0.20.2 TinyWL (CC0). See LICENSES/tinywl-CC0.txt. */
#ifndef SHADY_H
#define SHADY_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include <xkbcommon/xkbcommon.h>

#include "render/math3d.h"

struct wlr_allocator;
struct wlr_backend;
struct wlr_cursor;
struct wlr_output;
struct wlr_output_layout;
struct wlr_renderer;
struct wlr_seat;
struct wlr_surface;
struct wlr_xcursor_manager;
struct wlr_xdg_shell;
struct wlr_xdg_toplevel;
struct wlr_xdg_popup;
struct wlr_keyboard;
struct wlr_input_device;

enum shady_cursor_mode {
	SHADY_CURSOR_PASSTHROUGH,
	SHADY_CURSOR_MOVE,
	SHADY_CURSOR_RESIZE,
	SHADY_CURSOR_CAMERA_ORBIT,
	SHADY_CURSOR_CAMERA_PAN,
};

enum shady_close_state {
	SHADY_CLOSE_IDLE,
	SHADY_CLOSE_CRUMPLING,
	SHADY_CLOSE_WAITING,
	SHADY_CLOSE_RESTORING,

	/*
	 * The client survived the close request.
	 *
	 * The window is fully restored and interactive, but if it
	 * eventually unmaps we want to play the real exit animation
	 * from a compositor-owned snapshot.
	 */
	SHADY_CLOSE_ARMED,
};

struct shady_toplevel;

struct shady_server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;
	struct wlr_scene_output_layout *scene_layout;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener new_xdg_toplevel;
	struct wl_listener new_xdg_popup;
	struct wl_list toplevels;

	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *cursor_mgr;
	struct wl_listener cursor_motion;
	struct wl_listener cursor_motion_absolute;
	struct wl_listener cursor_button;
	struct wl_listener cursor_axis;
	struct wl_listener cursor_frame;

	struct wlr_seat *seat;
	struct wl_listener new_input;
	struct wl_listener request_cursor;
	struct wl_listener pointer_focus_change;
	struct wl_listener request_set_selection;
	struct wl_list keyboards;
	enum shady_cursor_mode cursor_mode;
	struct shady_toplevel *grabbed_toplevel;
	double grab_x, grab_y;
	struct wlr_box grab_geobox;
	uint32_t resize_edges;

	struct wlr_output_layout *output_layout;
	struct wl_list outputs;
	struct wl_listener new_output;

	struct shady_camera camera;
	double cam_grab_x, cam_grab_y;
	float cam_grab_yaw, cam_grab_pitch;
	float cam_grab_target_x, cam_grab_target_y, cam_grab_target_z;

	/* FPS controls are compositor-owned while first-person mode is active. */
	bool fps_forward, fps_back, fps_left, fps_right;
	bool fps_jump_queued;

	/* Window currently held by the first-person camera. */
	struct shady_toplevel *fps_held_toplevel;
	float fps_hold_distance;

	/* F3 toggles between navigation and normal client input in FPS mode. */
	bool fps_input_capture;

	/* Runtime world setting: released windows fall onto the floor when enabled. */
	bool window_gravity;
};

struct shady_output {
	struct wl_list link;
	struct shady_server *server;
	struct wlr_output *wlr_output;
	struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;
};

struct shady_toplevel {
	struct wl_list link;
	struct shady_server *server;
	struct wlr_xdg_toplevel *xdg_toplevel;
	struct wlr_scene_tree *scene_tree;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener commit;
	struct wl_listener destroy;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	float wobble_x;
	float wobble_y;

	float wobble_vx;
	float wobble_vy;

	/* Rigid-body 3D tilt, independent from the flexible shader wobble. */
	float tilt_x;
	float tilt_y;
	float tilt_vx;
	float tilt_vy;

	/* Per-window position on the real world Z axis. 0 = desktop plane. */
	float z;

	/* Vertical world-space velocity used by optional window gravity. */
	float physics_vy;

	double last_move_x;
	double last_move_y;

	bool wobble_dragging;

		/*
	 * Animated close state.
	 *
	 * Alt+F4 starts the animation instead of immediately sending the
	 * xdg_toplevel close event.
	 *
	 * close_progress:
	 *     0.0 = normal window
	 *     1.0 = fully crumpled
	 */
	enum shady_close_state close_state;
	float close_progress;
	float close_wait_time;
};

struct shady_popup {
	struct wlr_xdg_popup *xdg_popup;
	struct wl_listener commit;
	struct wl_listener destroy;
};

struct shady_keyboard {
	struct wl_list link;
	struct shady_server *server;
	struct wlr_keyboard *wlr_keyboard;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
};

/* Shared helpers used across compositor modules */
void focus_toplevel(struct shady_toplevel *toplevel);
void reset_cursor_mode(struct shady_server *server);

/* input.c */
void server_new_input(struct wl_listener *listener, void *data);
void seat_request_cursor(struct wl_listener *listener, void *data);
void seat_pointer_focus_change(struct wl_listener *listener, void *data);
void seat_request_set_selection(struct wl_listener *listener, void *data);
void server_cursor_motion(struct wl_listener *listener, void *data);
void server_cursor_motion_absolute(struct wl_listener *listener, void *data);
void server_cursor_button(struct wl_listener *listener, void *data);
void server_cursor_axis(struct wl_listener *listener, void *data);
void server_cursor_frame(struct wl_listener *listener, void *data);

/* output.c */
void server_new_output(struct wl_listener *listener, void *data);

/* xdg.c */
void server_new_xdg_toplevel(struct wl_listener *listener, void *data);
void server_new_xdg_popup(struct wl_listener *listener, void *data);

#endif
