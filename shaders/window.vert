attribute vec3 a_pos;

uniform mat4 u_mvp;

/*
 * Current spring displacement.
 *
 * Values are deliberately expressed in local window coordinates so
 * deformation behaves consistently regardless of window resolution.
 */
uniform vec2 u_wobble;

varying vec2 v_uv;

void main() {
	vec2 uv = a_pos.xy;

	v_uv = vec2(
		uv.x,
		1.0 - uv.y
	);

	vec3 pos = a_pos;

	/*
	 * Distance from the centre.
	 *
	 * We use smooth spatial waves instead of translating the whole
	 * rectangle. Because the window is rendered as a subdivided mesh,
	 * individual vertices can now bend independently.
	 */
	float cx = uv.x - 0.5;
	float cy = uv.y - 0.5;

	/*
	 * Horizontal movement bends vertical strips.
	 *
	 * The middle has the greatest freedom while the deformation changes
	 * smoothly toward the edges.
	 */
	float bend_x =
		sin(uv.y * 3.14159265);

	float bend_y =
		sin(uv.x * 3.14159265);

	/*
	 * Main jelly deformation.
	 */
	pos.x +=
		u_wobble.x *
		bend_x *
		(0.75 + 0.25 * cos(cy * 3.14159265));

	pos.y +=
		u_wobble.y *
		bend_y *
		(0.75 + 0.25 * cos(cx * 3.14159265));

	/*
	 * Cross-axis shear.
	 *
	 * This is what stops the effect looking like a simple sine wave and
	 * makes quick diagonal drags feel more rubbery.
	 */
	pos.x +=
		u_wobble.y *
		cy *
		0.18 *
		bend_y;

	pos.y +=
		u_wobble.x *
		cx *
		0.18 *
		bend_x;

	gl_Position =
		u_mvp *
		vec4(pos, 1.0);

	/*
	 * wlroots GLES output FBO orientation.
	 */
	gl_Position.y =
		-gl_Position.y;
}