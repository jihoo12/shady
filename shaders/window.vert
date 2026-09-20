attribute vec2 a_pos;

uniform mat3 u_proj;

varying vec2 v_uv;

void main() {
	/* a_pos is the unit quad (0..1). Match wlroots: UV == pos for untransformed textures. */
	v_uv = a_pos;
	gl_Position = vec4(vec3(a_pos, 1.0) * u_proj, 1.0);
}
