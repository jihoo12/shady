#ifndef SHADY_MODULE_FPS_H
#define SHADY_MODULE_FPS_H
#include "../../shady.h"
struct wlr_pointer_axis_event;
struct wlr_input_device;
#if SHADY_HAS_FPS
void shady_fps_host_init(struct shady_server *server);
void shady_fps_host_finish(struct shady_server *server);
void shady_fps_host_pointer_added(struct shady_server *server,struct wlr_input_device *device);
bool shady_fps_toggle(struct shady_server *server);
bool shady_fps_toggle_capture(struct shady_server *server);
bool shady_fps_handle_key(struct shady_server *server,const xkb_keysym_t *syms,int nsyms,uint32_t state);
bool shady_fps_handle_motion(struct shady_server *server,double dx,double dy);
bool shady_fps_handle_button(struct shady_server *server,uint32_t button,uint32_t state);
bool shady_fps_handle_axis(struct shady_server *server,struct wlr_pointer_axis_event *event);
void shady_fps_update(struct shady_server *server,float dt);
void shady_fps_update_held_window(struct shady_server *server,float logical_w,float logical_h);
void shady_fps_toplevel_gone(struct shady_server *server,struct shady_toplevel *toplevel);
bool shady_fps_is_holding(const struct shady_server *server,const struct shady_toplevel *toplevel);
#else
static inline void shady_fps_host_init(struct shady_server*s){(void)s;}
static inline void shady_fps_host_finish(struct shady_server*s){(void)s;}
static inline void shady_fps_host_pointer_added(struct shady_server*s,struct wlr_input_device*d){(void)s;(void)d;}
static inline bool shady_fps_is_holding(const struct shady_server*s,const struct shady_toplevel*t){(void)s;(void)t;return false;}
static inline bool shady_fps_toggle(struct shady_server*s){(void)s;return true;}
static inline bool shady_fps_toggle_capture(struct shady_server*s){(void)s;return false;}
static inline bool shady_fps_handle_key(struct shady_server*s,const xkb_keysym_t*y,int n,uint32_t st){(void)s;(void)y;(void)n;(void)st;return false;}
static inline bool shady_fps_handle_motion(struct shady_server*s,double x,double y){(void)s;(void)x;(void)y;return false;}
static inline bool shady_fps_handle_button(struct shady_server*s,uint32_t b,uint32_t st){(void)s;(void)b;(void)st;return false;}
static inline bool shady_fps_handle_axis(struct shady_server*s,struct wlr_pointer_axis_event*e){(void)s;(void)e;return false;}
static inline void shady_fps_update(struct shady_server*s,float dt){(void)s;(void)dt;}
static inline void shady_fps_update_held_window(struct shady_server*s,float w,float h){(void)s;(void)w;(void)h;}
static inline void shady_fps_toplevel_gone(struct shady_server*s,struct shady_toplevel*t){(void)s;(void)t;}
#endif
#endif
