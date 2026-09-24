#include "physics.h"
#include <math.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "../../render/render.h"
#define WINDOW_GRAVITY 2.8f
#define FLOOR_Y -0.62f

void shady_physics_init(struct shady_server *server) {
	server->physics.gravity_enabled=server->config.physics_enabled && server->config.window_gravity;
}
void shady_physics_toggle_gravity(struct shady_server *server) {
	if (!server->config.physics_enabled) return;
	server->physics.gravity_enabled=!server->physics.gravity_enabled;
	struct shady_toplevel *t;
	wl_list_for_each(t,&server->toplevels,link) t->physics_vy=0.f;
	shady_render_schedule_all_outputs(server);
}
void shady_physics_update(struct shady_server *server,float dt,float logical_w,float logical_h) {
	if(!server->config.physics_enabled || !server->physics.gravity_enabled || dt<=0.f || logical_w<=0.f || logical_h<=0.f) return;
	const float restitution=.22f, friction_rate=7.f, angular_kick=.22f;
	struct shady_toplevel *t;
	wl_list_for_each(t,&server->toplevels,link) {
		if(t==server->fps.held_toplevel){t->physics_vx=t->physics_vy=t->physics_vz=0.f;continue;}
		struct wlr_surface *surface=t->xdg_toplevel->base->surface;
		if(!surface->mapped)continue;
		float tw=(float)surface->current.width,th=(float)surface->current.height;
		if(tw<=0.f||th<=0.f)continue;
		float world_w=tw/logical_h,world_h=th/logical_h;
		float center_x=((float)t->scene_tree->node.x+tw*.5f-logical_w*.5f)/logical_h;
		float center_y=.5f-((float)t->scene_tree->node.y+th*.5f)/logical_h;
		float sx=sinf(t->tilt_x),cx=cosf(t->tilt_x),sy=sinf(t->tilt_y);
		float half_h=fabsf(cx)*world_h*.5f+fabsf(sx*sy)*world_w*.5f;
		if(half_h<.012f)half_h=.012f;
		t->physics_vy-=WINDOW_GRAVITY*dt; center_x+=t->physics_vx*dt; center_y+=t->physics_vy*dt; t->z+=t->physics_vz*dt;
		float floor_center=FLOOR_Y+half_h;
		if(center_y<=floor_center){
			float impact=-t->physics_vy;center_y=floor_center;
			if(impact>.12f){t->physics_vy=impact*restitution;float side=sinf(t->tilt_y)>=0.f?1.f:-1.f;t->tilt_vx+=side*impact*angular_kick;t->tilt_vy-=sinf(t->tilt_x)*impact*angular_kick;if(server->config.window_wobble){t->wobble_vx+=side*impact*.018f;t->wobble_vy+=impact*.035f;}}
			else t->physics_vy=0.f;
			float friction=1.f-friction_rate*dt;if(friction<0.f)friction=0.f;t->tilt_vx*=friction;t->tilt_vy*=friction;t->physics_vx*=friction;t->physics_vz*=friction;
		}
		int x=(int)(center_x*logical_h+logical_w*.5f-tw*.5f);int y=(int)((.5f-center_y)*logical_h-th*.5f);
		wlr_scene_node_set_position(&t->scene_tree->node,x,y);
	}
}
