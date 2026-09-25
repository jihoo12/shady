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
#include "../../world/world.h"
#define WINDOW_GRAVITY 2.8f

bool shady_physics_window_body(const struct shady_toplevel *t,float logical_w,float logical_h,struct shady_window_body *body){
	if(!t||!body||logical_h<=0.f)return false;
	struct wlr_surface*s=t->xdg_toplevel->base->surface;
	float tw=(float)s->current.width,th=(float)s->current.height;if(tw<=0.f||th<=0.f)return false;
	float ww=tw/logical_h,wh=th/logical_h,tx=0.f,ty=0.f;shady_window_motion_get_tilt(t,&tx,&ty);
	float sx=sinf(tx),cx=cosf(tx),sy=sinf(ty);
	body->center[0]=((float)t->scene_tree->node.x+tw*.5f-logical_w*.5f)/logical_h;
	body->center[1]=.5f-((float)t->scene_tree->node.y+th*.5f)/logical_h;
	body->center[2]=t->transform.z;body->tilt_x=tx;body->tilt_y=ty;
	body->half[0]=fabsf(cosf(ty))*ww*.5f;
	body->half[1]=fabsf(cx)*wh*.5f+fabsf(sx*sy)*ww*.5f;
	body->half[2]=fabsf(sy)*ww*.5f+fabsf(sx)*wh*.5f;
	if(body->half[0]<.006f) body->half[0]=.006f;
	if(body->half[1]<.012f) body->half[1]=.012f;
	if(body->half[2]<.006f) body->half[2]=.006f;
	return true;
}

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
		if(shady_fps_is_holding(server,t)||shady_fps_is_expanded(server,t)){shady_physics_stop(t);continue;}
		struct wlr_surface *surface=t->xdg_toplevel->base->surface;
		if(!surface->mapped)continue;
		float tw=(float)surface->current.width,th=(float)surface->current.height;
		if(tw<=0.f||th<=0.f)continue;
		/* Folded FPS windows are authoritative cubes. Collision deliberately
		 * ignores visual tilt so a wobble cannot shrink the support footprint or
		 * move the bottom face through the floor. */
		const float cube_size=SHADY_FPS_CUBE_SIZE;
		float center_x=((float)t->scene_tree->node.x+tw*.5f-logical_w*.5f)/logical_h;
		float center_y=.5f-((float)t->scene_tree->node.y+th*.5f)/logical_h;
		float half_x=cube_size*.5f,half_h=cube_size*.5f,half_z=cube_size*.5f;
		float previous_bottom = center_y - half_h;
		float previous_top = center_y + half_h;
		t->physics.vy-=WINDOW_GRAVITY*dt;
		float next_y=center_y+t->physics.vy*dt;

		/* Sweep the cube vertically as well as horizontally. This prevents a
		 * fast throw from crossing a thin authored OBJ slab in one frame. */
		for(size_t i=0;i<server->world.collider_count;i++){
			const struct shady_box_collider*b=&server->world.colliders[i];
			bool xz=center_x+half_x>b->min_x&&center_x-half_x<b->max_x&&
				t->transform.z+half_z>b->min_z&&t->transform.z-half_z<b->max_z;
			if(!xz)continue;
			if(t->physics.vy<0.f&&previous_bottom>=b->max_y&&next_y-half_h<=b->max_y){
				next_y=b->max_y+half_h;
				t->physics.vy=-t->physics.vy*restitution;
				break;
			}
			if(t->physics.vy>0.f&&previous_top<=b->min_y&&next_y+half_h>=b->min_y){
				next_y=b->min_y-half_h;
				t->physics.vy=-t->physics.vy*restitution;
				break;
			}
		}
		center_y=next_y;

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
		/* Keep resting windows attached to a support despite tiny frame-to-frame
		 * body/tilt changes. For larger gaps still require an actual downward
		 * crossing so elevated colliders cannot pull a window upward. */
		if (t->physics.vy <= 0.f) {
			const float contact_slop=.025f;
			for (size_t i=0;i<server->world.collider_count;i++) {
				const struct shady_box_collider *b=&server->world.colliders[i];
				float y=b->max_y;
				if(!shady_box_overlap_xz(b,&body))continue;
				bool crossed=previous_bottom>=y && body.min_y<=y;
				bool resting=previous_bottom>=y-contact_slop &&
					previous_bottom<=y+contact_slop && body.min_y<=y+contact_slop;
				if((crossed||resting)&&(!supported||y>support_y)){support_y=y;supported=true;}
			}
		}
		float floor_center=support_y+half_h;
		if(supported){
			float impact=-t->physics.vy;center_y=floor_center;
			if(impact>.12f){
				t->physics.vy=impact*restitution;
				/* Folded cubes have no tilt-dependent collision body. Keep the
				 * landing kick deterministic and let window motion animate it. */
				float side=t->physics.vx>=0.f?1.f:-1.f;
				shady_window_motion_add_impulse(server,t,side*impact*.018f,
					impact*.035f,side*impact*angular_kick,0.f);
			}
			else t->physics.vy=0.f;
			float friction=1.f-friction_rate*dt;if(friction<0.f)friction=0.f;shady_window_motion_apply_damping(t,friction);t->physics.vx*=friction;t->physics.vz*=friction;
		}
		int x=(int)(center_x*logical_h+logical_w*.5f-tw*.5f);int y=(int)((.5f-center_y)*logical_h-th*.5f);
		wlr_scene_node_set_position(&t->scene_tree->node,x,y);
	}
}

void shady_physics_set_velocity(struct shady_toplevel *toplevel,float vx,float vy,float vz){toplevel->physics.vx=vx;toplevel->physics.vy=vy;toplevel->physics.vz=vz;}
void shady_physics_stop(struct shady_toplevel *toplevel){shady_physics_set_velocity(toplevel,0.f,0.f,0.f);}
