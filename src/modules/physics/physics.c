#include "physics.h"
#include <math.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "../../render/render.h"
#include "../window_motion/window_motion.h"
#include "../fps/fps.h"
#include "../../world/floor.h"
#define WINDOW_GRAVITY 2.8f

void shady_physics_init(struct shady_server *server) {
	server->physics.gravity_enabled=server->config.physics_enabled && server->config.window_gravity;
}
void shady_physics_toggle_gravity(struct shady_server *server) {
	if (!server->config.physics_enabled) return;
	server->physics.gravity_enabled=!server->physics.gravity_enabled;
	struct shady_toplevel *t;
	wl_list_for_each(t,&server->toplevels,link) t->physics.vy=0.f;
	shady_render_schedule_all_outputs(server);
}
void shady_physics_update(struct shady_server *server,float dt,float logical_w,float logical_h) {
	if(!server->config.physics_enabled || !server->physics.gravity_enabled || !server->camera.first_person || dt<=0.f || logical_w<=0.f || logical_h<=0.f) return;
	const float restitution=.22f, friction_rate=7.f, angular_kick=.22f;
	const struct shady_floor floor=shady_world_floor();
	struct shady_toplevel *t;
	wl_list_for_each(t,&server->toplevels,link) {
		if(shady_fps_is_holding(server,t)){shady_physics_stop(t);continue;}
		struct wlr_surface *surface=t->xdg_toplevel->base->surface;
		if(!surface->mapped)continue;
		float tw=(float)surface->current.width,th=(float)surface->current.height;
		if(tw<=0.f||th<=0.f)continue;
		float world_w=tw/logical_h,world_h=th/logical_h;
		float center_x=((float)t->scene_tree->node.x+tw*.5f-logical_w*.5f)/logical_h;
		float center_y=.5f-((float)t->scene_tree->node.y+th*.5f)/logical_h;
		float tilt_x=0.f,tilt_y=0.f;shady_window_motion_get_tilt(t,&tilt_x,&tilt_y);
		float sx=sinf(tilt_x),cx=cosf(tilt_x),sy=sinf(tilt_y);
		float half_h=fabsf(cx)*world_h*.5f+fabsf(sx*sy)*world_w*.5f;
		if(half_h<.012f)half_h=.012f;
		/* Conservative XZ footprint of the tilted window. This keeps collision
		 * active while any part of the window still overlaps the finite floor. */
		float half_x=fabsf(cosf(tilt_y))*world_w*.5f;
		float half_z=fabsf(sinf(tilt_y))*world_w*.5f + fabsf(sinf(tilt_x))*world_h*.5f;
		if(half_x<.006f)half_x=.006f;
		if(half_z<.006f)half_z=.006f;
		t->physics.vy-=WINDOW_GRAVITY*dt; center_x+=t->physics.vx*dt; center_y+=t->physics.vy*dt; t->transform.z+=t->physics.vz*dt;
		float floor_center=floor.y+half_h;
		/* The floor is finite: outside its X/Z footprint the window keeps falling. */
		bool over_floor = center_x + half_x >= floor.min_x &&
			center_x - half_x <= floor.max_x &&
			t->transform.z + half_z >= floor.min_z &&
			t->transform.z - half_z <= floor.max_z;
		if(over_floor && center_y<=floor_center){
			float impact=-t->physics.vy;center_y=floor_center;
			if(impact>.12f){t->physics.vy=impact*restitution;float side=sinf(tilt_y)>=0.f?1.f:-1.f;shady_window_motion_add_impulse(server,t,side*impact*.018f,impact*.035f,side*impact*angular_kick,-sinf(tilt_x)*impact*angular_kick);}
			else t->physics.vy=0.f;
			float friction=1.f-friction_rate*dt;if(friction<0.f)friction=0.f;shady_window_motion_apply_damping(t,friction);t->physics.vx*=friction;t->physics.vz*=friction;
		}
		int x=(int)(center_x*logical_h+logical_w*.5f-tw*.5f);int y=(int)((.5f-center_y)*logical_h-th*.5f);
		wlr_scene_node_set_position(&t->scene_tree->node,x,y);
	}
}

void shady_physics_set_velocity(struct shady_toplevel *toplevel,float vx,float vy,float vz){toplevel->physics.vx=vx;toplevel->physics.vy=vy;toplevel->physics.vz=vz;}
void shady_physics_stop(struct shady_toplevel *toplevel){shady_physics_set_velocity(toplevel,0.f,0.f,0.f);}
