precision mediump float;

uniform sampler2D u_tex;
uniform vec4 u_tint;

/* 1.0 = sample alpha; 0.0 = force opaque (RGBX / XRGB). */
uniform float u_has_alpha;

/* Seconds since compositor render timer started. */
uniform float u_time;

varying vec2 v_uv;

void main() {
	vec2 uv = v_uv;

	/*
	 * Subtle animated chromatic aberration.
	 *
	 * Keep this deliberately small: we're rendering real application
	 * contents, so text should remain readable.
	 */
	float pulse = 0.5 + 0.5 * sin(u_time * 1.7);
	float aberration = 0.0007 + 0.0013 * pulse;

	vec4 center = texture2D(u_tex, uv);

	float r = texture2D(
		u_tex,
		clamp(uv + vec2(aberration, 0.0), 0.0, 1.0)
	).r;

	float g = center.g;

	float b = texture2D(
		u_tex,
		clamp(uv - vec2(aberration, 0.0), 0.0, 1.0)
	).b;

	vec4 color = vec4(r, g, b, center.a);

	if (u_has_alpha < 0.5) {
		color.a = 1.0;
	}

	/*
	 * Distance from the closest edge of the window.
	 */
	float edge_distance = min(
		min(uv.x, 1.0 - uv.x),
		min(uv.y, 1.0 - uv.y)
	);

	/*
	 * Thin outer glow.
	 */
	float edge = 1.0 - smoothstep(
		0.0,
		0.025,
		edge_distance
	);

	/*
	 * Slightly wider secondary glow.
	 */
	float soft_edge = 1.0 - smoothstep(
		0.0,
		0.09,
		edge_distance
	);

	/*
	 * Slowly shifting neon color.
	 */
	vec3 neon_a = vec3(0.10, 0.45, 1.00);
	vec3 neon_b = vec3(0.75, 0.15, 1.00);

	float wave = 0.5 + 0.5 * sin(
		u_time * 1.25 +
		uv.y * 6.0 +
		uv.x * 2.0
	);

	vec3 neon = mix(neon_a, neon_b, wave);

	/*
	 * Strong thin edge + subtle halo.
	 */
	color.rgb += neon * edge * (0.08 + 0.05 * pulse);
	color.rgb += neon * soft_edge * 0.018;

	/*
	 * Very subtle scanline modulation.
	 *
	 * UV-based rather than pixel-based so no resolution uniform is needed.
	 */
	float scan = 0.985 + 0.015 * sin(
		uv.y * 900.0 + u_time * 2.0
	);

	color.rgb *= scan;

	/*
	 * Preserve the project's original cool tint.
	 */
	color *= u_tint;

	gl_FragColor = color;
}