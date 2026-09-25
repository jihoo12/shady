#include "physics.h"
#include <math.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "../../render/render.h"
#include "../window_motion/window_motion.h"
#include "../fps/fps.h"
#include "../../world/floor.h"
#include "../../world/collider.h"
#include "../../world/platform.h"
#include "../../world/world.h"
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
		float previous_bottom = center_y - half_h;
		t->physics.vy-=WINDOW_GRAVITY*dt;
		center_y+=t->physics.vy*dt;

		/* Resolve horizontal motion one axis at a time. Using the previous
		 * leading face makes thin authored OBJ wall proxies much harder to
		 * tunnel through than an overlap-only test. */
		float old_x=center_x, old_z=t->transform.z;
		float next_x=center_x+t->physics.vx*dt;
		for(size_t i=0;i<server->world.collider_count;i++){
			const struct shady_box_collider*b=&server->world.colliders[i];
			bool yz=center_y+half_h>b->min_y&&center_y-half_h<b->max_y&&
				old_z+half_z>b->min_z&&old_z-half_z<b->max_z;
			if(!yz)continue;
			if(t->physics.vx>0.f&&old_x+half_x<=b->min_x&&next_x+half_x>=b->min_x){
				next_x=b->min_x-half_x;t->physics.vx=-t->physics.vx*restitution;
				shady_window_motion_add_impulse(server,t,-.018f,0.f,0.f,-angular_kick);break;
			}
			if(t->physics.vx<0.f&&old_x-half_x>=b->max_x&&next_x-half_x<=b->max_x){
				next_x=b->max_x+half_x;t->physics.vx=-t->physics.vx*restitution;
				shady_window_motion_add_impulse(server,t,.018f,0.f,0.f,angular_kick);break;
			}
		}
		center_x=next_x;
		float next_z=old_z+t->physics.vz*dt;
		for(size_t i=0;i<server->world.collider_count;i++){
			const struct shady_box_collider*b=&server->world.colliders[i];
			bool xy=center_y+half_h>b->min_y&&center_y-half_h<b->max_y&&
				center_x+half_x>b->min_x&&center_x-half_x<b->max_x;
			if(!xy)continue;
			if(t->physics.vz>0.f&&old_z+half_z<=b->min_z&&next_z+half_z>=b->min_z){
				next_z=b->min_z-half_z;t->physics.vz=-t->physics.vz*restitution;
				shady_window_motion_add_impulse(server,t,0.f,.018f,angular_kick,0.f);break;
			}
			if(t->physics.vz<0.f&&old_z-half_z>=b->max_z&&next_z-half_z<=b->max_z){
				next_z=b->max_z+half_z;t->physics.vz=-t->physics.vz*restitution;
				shady_window_motion_add_impulse(server,t,0.f,-.018f,-angular_kick,0.f);break;
			}
		}
		t->transform.z=next_z;
		struct shady_box_collider body={
			center_x-half_x,center_x+half_x,
			center_y-half_h,center_y+half_h,
			t->transform.z-half_z,t->transform.z+half_z
		};
		float support_y=0.f; bool supported=false;
		/* Only land on a surface crossed while descending. XZ overlap alone must
		 * never teleport a window upward onto an elevated platform. */
		if (t->physics.vy <= 0.f) {
			for (size_t i=0;i<server->world.collider_count;i++) {
				const struct shady_box_collider *b=&server->world.colliders[i];
				float y=b->max_y;
				if(previous_bottom>=y && body.min_y<=y && shady_box_overlap_xz(b,&body) &&
						(!supported || y>support_y)){support_y=y;supported=true;}
			}
		}
		float floor_center=support_y+half_h;
		if(supported){
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
