#include "fps.h"
#include <linux/input-event-codes.h>
#include <math.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/backend/wayland.h>
#include <wlr/types/wlr_input_device.h>
#include <string.h>
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "../../render/math3d.h"
#include "../../render/pick3d.h"
#include "../../render/render.h"
#include "../physics/physics.h"
#include "../window_motion/window_motion.h"
static void host_lock_destroy(struct shady_server*s){if(s->fps.host_lock){zwp_locked_pointer_v1_destroy(s->fps.host_lock);s->fps.host_lock=NULL;}}
static void host_lock_update(struct shady_server*s){
	if(!s->camera.first_person||!s->fps.input_capture){host_lock_destroy(s);return;}
	if(s->fps.host_lock||!s->fps.host_constraints||!s->fps.host_pointer)return;
	struct wlr_output*o=wlr_output_layout_output_at(s->output_layout,s->cursor->x,s->cursor->y);
	if(!o&&!wl_list_empty(&s->outputs)){struct shady_output*out=wl_container_of(s->outputs.next,out,link);o=out->wlr_output;}
	if(!o||!wlr_output_is_wl(o))return;
	struct wl_surface*surface=wlr_wl_output_get_surface(o);
	s->fps.host_lock=zwp_pointer_constraints_v1_lock_pointer(s->fps.host_constraints,surface,s->fps.host_pointer,NULL,ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	wl_surface_commit(surface);
}
static void host_registry_global(void*data,struct wl_registry*r,uint32_t name,const char*iface,uint32_t version){struct shady_server*s=data;if(strcmp(iface,zwp_pointer_constraints_v1_interface.name)==0&&!s->fps.host_constraints)s->fps.host_constraints=wl_registry_bind(r,name,&zwp_pointer_constraints_v1_interface,version>1?1:version);}
static void host_registry_remove(void*data,struct wl_registry*r,uint32_t name){(void)data;(void)r;(void)name;}
static const struct wl_registry_listener host_registry_listener={.global=host_registry_global,.global_remove=host_registry_remove};
void shady_fps_host_init(struct shady_server*s){if(!wlr_backend_is_wl(s->backend))return;struct wl_display*d=wlr_wl_backend_get_remote_display(s->backend);s->fps.host_registry=wl_display_get_registry(d);wl_registry_add_listener(s->fps.host_registry,&host_registry_listener,s);wl_display_roundtrip(d);}
void shady_fps_host_pointer_added(struct shady_server*s,struct wlr_input_device*d){if(!wlr_input_device_is_wl(d)||s->fps.host_pointer)return;struct wl_seat*seat=wlr_wl_input_device_get_seat(d);s->fps.host_pointer=wl_seat_get_pointer(seat);host_lock_update(s);}
void shady_fps_host_finish(struct shady_server*s){host_lock_destroy(s);if(s->fps.host_pointer){wl_pointer_release(s->fps.host_pointer);s->fps.host_pointer=NULL;}if(s->fps.host_constraints){zwp_pointer_constraints_v1_destroy(s->fps.host_constraints);s->fps.host_constraints=NULL;}if(s->fps.host_registry){wl_registry_destroy(s->fps.host_registry);s->fps.host_registry=NULL;}}
#define LOOK_SENS .0032f
#define HOLD_MIN .28f
#define HOLD_MAX 2.50f
#define FLOOR_Y -.62f
#define EYE_HEIGHT .40f
#define MOVE_SPEED 1.25f
#define GRAVITY 3.8f
#define JUMP_SPEED 1.45f
static void clamp_pitch(struct shady_camera*c){if(c->pitch>1.4f)c->pitch=1.4f;if(c->pitch<-1.4f)c->pitch=-1.4f;}
static void center_cursor(struct shady_server*s){
	struct wlr_output*o=wlr_output_layout_output_at(s->output_layout,s->cursor->x,s->cursor->y);
	if(!o&&!wl_list_empty(&s->outputs)){struct shady_output*out=wl_container_of(s->outputs.next,out,link);o=out->wlr_output;}
	if(!o)return;
	double ox=0,oy=0;wlr_output_layout_output_coords(s->output_layout,o,&ox,&oy);
	float sc=o->scale>0?o->scale:1.f;
	wlr_cursor_warp(s->cursor,NULL,ox+(double)o->width/sc*.5,oy+(double)o->height/sc*.5);
}
bool shady_fps_toggle(struct shady_server*s){if(!s->config.fps_mode)return true;s->camera.first_person=!s->camera.first_person;s->fps.forward=s->fps.back=s->fps.left=s->fps.right=false;s->fps.jump_queued=false;s->fps.held_toplevel=NULL;s->fps.input_capture=s->camera.first_person;if(s->camera.first_person){struct shady_vec3 eye;shady_camera_eye(&s->camera,&eye);s->camera.pos_x=eye.x;s->camera.pos_y=eye.y;s->camera.pos_z=eye.z;s->camera.vel_y=0;s->camera.grounded=false;wlr_seat_pointer_clear_focus(s->seat);center_cursor(s);}host_lock_update(s);shady_render_schedule_all_outputs(s);return true;
bool shady_fps_toggle_capture(struct shady_server*s){if(!s->camera.first_person)return false;s->fps.input_capture=!s->fps.input_capture;s->fps.forward=s->fps.back=s->fps.left=s->fps.right=false;s->fps.jump_queued=false;if(s->fps.input_capture){wlr_seat_pointer_clear_focus(s->seat);center_cursor(s);}host_lock_update(s);shady_render_schedule_all_outputs(s);return true;}
bool shady_fps_handle_key(struct shady_server*s,const xkb_keysym_t*syms,int n,uint32_t state){if(!s->camera.first_person||!s->fps.input_capture)return false;bool p=state==WL_KEYBOARD_KEY_STATE_PRESSED,handled=false;for(int j=0;j<n;j++)switch(syms[j]){case XKB_KEY_w:case XKB_KEY_W:s->fps.forward=p;handled=true;break;case XKB_KEY_s:case XKB_KEY_S:s->fps.back=p;handled=true;break;case XKB_KEY_a:case XKB_KEY_A:s->fps.left=p;handled=true;break;case XKB_KEY_d:case XKB_KEY_D:s->fps.right=p;handled=true;break;case XKB_KEY_space:if(p)s->fps.jump_queued=true;handled=true;break;default:break;}return handled;}
bool shady_fps_handle_motion(struct shady_server*s,double dx,double dy){if(!s->camera.first_person||!s->fps.input_capture)return false;s->camera.yaw-=(float)dx*LOOK_SENS;s->camera.pitch-=(float)dy*LOOK_SENS;clamp_pitch(&s->camera);struct wlr_output*o=wlr_output_layout_output_at(s->output_layout,s->cursor->x,s->cursor->y);if(o){double ox=0,oy=0;wlr_output_layout_output_coords(s->output_layout,o,&ox,&oy);float sc=o->scale>0?o->scale:1;wlr_cursor_warp(s->cursor,NULL,-ox+(double)o->width/sc*.5,-oy+(double)o->height/sc*.5);}wlr_seat_pointer_clear_focus(s->seat);shady_render_schedule_all_outputs(s);return true;}
bool shady_fps_handle_button(struct shady_server*s,uint32_t button,uint32_t state){if(!s->camera.first_person||!s->fps.input_capture)return false;if(button==BTN_RIGHT&&state==WL_POINTER_BUTTON_STATE_PRESSED&&s->fps.held_toplevel){struct shady_vec3 f;shady_camera_basis(&s->camera,NULL,NULL,&f);struct shady_toplevel*t=s->fps.held_toplevel;float v=2.6f;shady_physics_set_velocity(t,f.x*v,f.y*v+s->camera.vel_y,f.z*v);shady_window_motion_add_impulse(s,t,f.x*.035f,f.y*.035f,-f.y*1.1f,f.x*.7f);s->fps.held_toplevel=NULL;shady_render_schedule_all_outputs(s);return true;}if(button==BTN_LEFT&&state==WL_POINTER_BUTTON_STATE_PRESSED){if(s->fps.held_toplevel)s->fps.held_toplevel=NULL;else{float d=0;struct shady_toplevel*t=shady_toplevel_at_camera_center(s,&d);if(t&&d<=HOLD_MAX){s->fps.held_toplevel=t;s->fps.hold_distance=d;if(s->fps.hold_distance<HOLD_MIN)s->fps.hold_distance=HOLD_MIN;focus_toplevel(t);}}shady_render_schedule_all_outputs(s);}return true;}
bool shady_fps_handle_axis(struct shady_server*s,struct wlr_pointer_axis_event*e){if(!s->camera.first_person||!s->fps.input_capture||!s->fps.held_toplevel||e->orientation!=WL_POINTER_AXIS_VERTICAL_SCROLL)return false;s->fps.hold_distance+=(float)e->delta*.0025f;if(s->fps.hold_distance<HOLD_MIN)s->fps.hold_distance=HOLD_MIN;if(s->fps.hold_distance>HOLD_MAX)s->fps.hold_distance=HOLD_MAX;shady_render_schedule_all_outputs(s);return true;}
void shady_fps_update(struct shady_server*s,float dt){struct shady_camera*c=&s->camera;if(!c->first_person)return;float sy=sinf(c->yaw),cy=cosf(c->yaw),fx=-sy,fz=-cy,rx=cy,rz=-sy,mx=0,mz=0;if(s->fps.forward){mx+=fx;mz+=fz;}if(s->fps.back){mx-=fx;mz-=fz;}if(s->fps.right){mx+=rx;mz+=rz;}if(s->fps.left){mx-=rx;mz-=rz;}float ml=sqrtf(mx*mx+mz*mz);if(ml>.001f){c->pos_x+=mx/ml*MOVE_SPEED*dt;c->pos_z+=mz/ml*MOVE_SPEED*dt;}if(s->fps.jump_queued&&c->grounded){c->vel_y=JUMP_SPEED;c->grounded=false;}s->fps.jump_queued=false;c->vel_y-=GRAVITY*dt;c->pos_y+=c->vel_y*dt;float y=FLOOR_Y+EYE_HEIGHT;if(c->pos_y<=y){c->pos_y=y;c->vel_y=0;c->grounded=true;}}
void shady_fps_update_held_window(struct shady_server*s,float lw,float lh){struct shady_toplevel*t=s->fps.held_toplevel;if(!s->camera.first_person||!t)return;struct wlr_surface*surface=t->xdg_toplevel->base->surface;if(!surface->mapped||lh<=0){s->fps.held_toplevel=NULL;return;}struct shady_vec3 eye,f;shady_camera_eye(&s->camera,&eye);shady_camera_basis(&s->camera,NULL,NULL,&f);float d=s->fps.hold_distance,cx=eye.x+f.x*d,cy=eye.y+f.y*d,cz=eye.z+f.z*d,tw=(float)surface->current.width,th=(float)surface->current.height,ww=tw/lh,wh=th/lh;int x=(int)((cx-ww*.5f)*lh+lw*.5f),y=(int)(lh*.5f-(cy-wh*.5f+wh)*lh);wlr_scene_node_set_position(&t->scene_tree->node,x,y);t->transform.z=cz;t->motion.tilt_x=-s->camera.pitch;t->motion.tilt_y=s->camera.yaw;t->motion.tilt_vx=t->motion.tilt_vy=0;if(s->config.window_wobble){t->motion.wobble_vx+=f.x*.00045f;t->motion.wobble_vy+=f.y*.00045f;}}
void shady_fps_toplevel_gone(struct shady_server*s,struct shady_toplevel*t){if(s->fps.held_toplevel==t)s->fps.held_toplevel=NULL;}

bool shady_fps_is_holding(const struct shady_server*s,const struct shady_toplevel*t){return s->fps.held_toplevel==t;}
