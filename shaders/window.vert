attribute vec3 a_pos;

uniform mat4 u_mvp;

varying vec2 v_uv;

void main() {
	/* Local (0,0)=bottom-left; Wayland buffers are top-left → flip V. */
	v_uv = vec2(a_pos.x, 1.0 - a_pos.y);
	gl_Position = u_mvp * vec4(a_pos, 1.0);
	/*
	 * wlroots GLES output FBOs are presented with top-left origin (same as
	 * their FLIPPED_180 2D path). Standard GL NDC +Y is the opposite, so
	 * flip clip-space Y or the whole desktop appears upside-down.
	 */
	gl_Position.y = -gl_Position.y;
}
