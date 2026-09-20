#extension GL_OES_EGL_image_external : require
precision mediump float;

uniform samplerExternalOES u_tex;
uniform vec4 u_tint;
uniform float u_has_alpha;

varying vec2 v_uv;

void main() {
	vec4 color = texture2D(u_tex, v_uv);
	if (u_has_alpha < 0.5) {
		color.a = 1.0;
	}
	gl_FragColor = color * u_tint;
}
