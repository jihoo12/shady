attribute vec3 a_pos;

uniform mat4 u_mvp;

uniform vec2 u_wobble;

/*
 * 0.0 = normal
 * 1.0 = completely crumpled
 */
uniform float u_close_progress;

varying vec2 v_uv;

void main() {
	vec2 uv = a_pos.xy;

	v_uv = vec2(
		uv.x,
		1.0 - uv.y
	);

	vec3 pos = a_pos;

	float cx =
		uv.x - 0.5;

	float cy =
		uv.y - 0.5;

	/*
	 * ------------------------------------------------------------
	 * Existing wobbly-window deformation
	 * ------------------------------------------------------------
	 */

	float bend_x =
		sin(
			uv.y *
			3.14159265
		);

	float bend_y =
		sin(
			uv.x *
			3.14159265
		);

	pos.x +=
		u_wobble.x *
		bend_x *
		(
			0.75 +
			0.25 *
			cos(
				cy *
				3.14159265
			)
		);

	pos.y +=
		u_wobble.y *
		bend_y *
		(
			0.75 +
			0.25 *
			cos(
				cx *
				3.14159265
			)
		);

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

	/*
	 * ------------------------------------------------------------
	 * Close / crumple animation
	 * ------------------------------------------------------------
	 */

	float p =
		clamp(
			u_close_progress,
			0.0,
			1.0
		);

	/*
	 * Ease-in.
	 *
	 * The first part moves slowly, then the collapse accelerates.
	 */
	float collapse =
		p * p;

	/*
	 * Pull every vertex toward the centre.
	 *
	 * At the end we leave a tiny amount of size so triangles do not
	 * become numerically degenerate too early.
	 */
	float shrink =
		1.0 -
		collapse *
		0.94;

	vec2 centred =
		pos.xy -
		vec2(
			0.5,
			0.5
		);

	centred *=
		shrink;

	pos.xy =
		centred +
		vec2(
			0.5,
			0.5
		);

	/*
	 * Crumpling waves.
	 *
	 * Their amplitude grows during the middle of the animation and
	 * disappears again as the window reaches its final tiny shape.
	 */
	float wrinkle_envelope =
		sin(
			p *
			3.14159265
		);

	float wrinkle1 =
		sin(
			uv.x * 31.0 +
			uv.y * 17.0 +
			p * 11.0
		);

	float wrinkle2 =
		cos(
			uv.x * 19.0 -
			uv.y * 29.0 -
			p * 8.0
		);

	float wrinkle3 =
		sin(
			(uv.x + uv.y) *
			37.0 +
			p * 14.0
		);

	pos.x +=
		wrinkle1 *
		0.055 *
		wrinkle_envelope;

	pos.y +=
		wrinkle2 *
		0.045 *
		wrinkle_envelope;

	/*
	 * Push alternating mesh regions forward/backward in Z.
	 *
	 * This is what gives the paper-fold / crushed-sheet appearance
	 * when the compositor camera is rotated.
	 */
	pos.z +=
		(
			wrinkle1 *
			wrinkle2 +
			wrinkle3 * 0.5
		) *
		0.055 *
		wrinkle_envelope;

	/*
	 * Twist the shrinking window around its centre.
	 */
	float angle =
		p *
		p *
		1.15;

	float s =
		sin(angle);

	float c =
		cos(angle);

	vec2 rotate_pos =
		pos.xy -
		vec2(
			0.5,
			0.5
		);

	rotate_pos =
		vec2(
			rotate_pos.x * c -
			rotate_pos.y * s,

			rotate_pos.x * s +
			rotate_pos.y * c
		);

	pos.xy =
		rotate_pos +
		vec2(
			0.5,
			0.5
		);

	/*
	 * Final "suction" deformation.
	 *
	 * Vertices near the outside are pulled slightly harder than the
	 * centre, giving the last few frames a crushed-ball appearance.
	 */
	float radius =
		length(
			vec2(
				cx,
				cy
			)
		);

	float suction =
		smoothstep(
			0.45,
			1.0,
			p
		);

	pos.x +=
		cx *
		radius *
		0.12 *
		suction;

	pos.y -=
		cy *
		radius *
		0.08 *
		suction;

	/*
	 * Pull the final object slightly toward the camera.
	 */
	pos.z +=
		p *
		p *
		0.08;

	gl_Position =
		u_mvp *
		vec4(
			pos,
			1.0
		);

	gl_Position.y =
		-gl_Position.y;
}