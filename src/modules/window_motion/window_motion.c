#include "window_motion.h"
#include <math.h>

void shady_window_motion_update_toplevel(struct shady_server *server,
		struct shady_toplevel *toplevel, float dt) {
	if (dt <= 0.f) return;

	/* Flexible wobble is a configurable visual effect. When disabled, clear
	 * all accumulated state so other modules cannot leave a latent impulse. */
	if (server->config.window_wobble) {
		const float spring = 42.f, damping_rate = 7.5f;
		toplevel->motion.wobble_vx += -toplevel->motion.wobble_x * spring * dt;
		toplevel->motion.wobble_vy += -toplevel->motion.wobble_y * spring * dt;
		float damping = 1.f - damping_rate * dt;
		if (damping < 0.f) damping = 0.f;
		toplevel->motion.wobble_vx *= damping;
		toplevel->motion.wobble_vy *= damping;
		toplevel->motion.wobble_x += toplevel->motion.wobble_vx * dt;
		toplevel->motion.wobble_y += toplevel->motion.wobble_vy * dt;
		if (fabsf(toplevel->motion.wobble_x)<.00005f && fabsf(toplevel->motion.wobble_vx)<.00005f)
			toplevel->motion.wobble_x=toplevel->motion.wobble_vx=0.f;
		if (fabsf(toplevel->motion.wobble_y)<.00005f && fabsf(toplevel->motion.wobble_vy)<.00005f)
			toplevel->motion.wobble_y=toplevel->motion.wobble_vy=0.f;
	} else {
		toplevel->motion.wobble_x=toplevel->motion.wobble_y=0.f;
		toplevel->motion.wobble_vx=toplevel->motion.wobble_vy=0.f;
	}

	/* Rigid tilt is intentionally independent from flexible wobble. */
	const float tilt_damping_rate = 9.f;
	float damping = 1.f - tilt_damping_rate * dt;
	if (damping < 0.f) damping = 0.f;
	toplevel->motion.tilt_vx *= damping;
	toplevel->motion.tilt_vy *= damping;
	toplevel->motion.tilt_x += toplevel->motion.tilt_vx * dt;
	toplevel->motion.tilt_y += toplevel->motion.tilt_vy * dt;
}
