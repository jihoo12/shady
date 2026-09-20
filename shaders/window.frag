precision mediump float;

uniform sampler2D u_tex;
uniform vec4 u_tint;
/* 1.0 = sample alpha; 0.0 = force opaque (RGBX / XRGB). */
uniform float u_has_alpha;

varying vec2 v_uv;

void main() {
	vec4 color = texture2D(u_tex, v_uv);
	if (u_has_alpha < 0.5) {
		color.a = 1.0;
	}
	gl_FragColor = color * u_tint;
}
