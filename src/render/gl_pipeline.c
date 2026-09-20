#include "gl_pipeline.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/egl.h>
#include <wlr/render/gles2.h>
#include <wlr/util/log.h>
#include <EGL/egl.h>
#include <GLES2/gl2ext.h>

#ifndef SHADY_SHADER_DIR
#define SHADY_SHADER_DIR "shaders"
#endif

/* Mild blue tint so custom GLSL is visibly different from stock compositing. */
static const float WINDOW_TINT[4] = { 0.85f, 0.90f, 1.10f, 1.0f };

static char *read_shader_file(const char *name) {
	char path[512];
	int n = snprintf(path, sizeof(path), "%s/%s", SHADY_SHADER_DIR, name);
	if (n < 0 || (size_t)n >= sizeof(path)) {
		wlr_log(WLR_ERROR, "shader path too long for %s", name);
		return NULL;
	}

	FILE *f = fopen(path, "rb");
	if (!f) {
		wlr_log(WLR_ERROR, "failed to open shader %s", path);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	long len = ftell(f);
	if (len < 0) {
		fclose(f);
		return NULL;
	}
	rewind(f);

	char *buf = malloc((size_t)len + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	size_t nread = fread(buf, 1, (size_t)len, f);
	fclose(f);
	if (nread != (size_t)len) {
		free(buf);
		return NULL;
	}
	buf[len] = '\0';
	return buf;
}

static GLuint compile_shader(GLenum type, const char *source, const char *label) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		wlr_log(WLR_ERROR, "failed to compile %s: %s", label, log);
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint link_program(const char *vert_src, const char *frag_src,
		const char *label) {
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vert_src, "vertex");
	if (!vs) {
		return 0;
	}
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_src, label);
	if (!fs) {
		glDeleteShader(vs);
		return 0;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glBindAttribLocation(prog, 0, "a_pos");
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint ok = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[512];
		glGetProgramInfoLog(prog, sizeof(log), NULL, log);
		wlr_log(WLR_ERROR, "failed to link %s: %s", label, log);
		glDeleteProgram(prog);
		return 0;
	}
	return prog;
}

static bool make_egl_current(struct wlr_renderer *renderer) {
	struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer);
	if (!egl) {
		return false;
	}
	EGLDisplay dpy = wlr_egl_get_display(egl);
	EGLContext ctx = wlr_egl_get_context(egl);
	if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
		wlr_log(WLR_ERROR, "eglMakeCurrent failed");
		return false;
	}
	return true;
}

/*
 * Matrices match wlroots util/matrix.c (row-major floats + `pos * mat` in GLSL
 * with glUniformMatrix3fv(..., GL_FALSE, ...)). Projection uses the same
 * FLIPPED_180 convention as gles2 begin_gles2_buffer_pass.
 */
static void projection_flipped_180(float mat[9], int width, int height) {
	float x = 2.0f / (float)width;
	float y = 2.0f / (float)height;
	memset(mat, 0, sizeof(float) * 9);
	mat[0] = x;
	mat[4] = y;
	mat[2] = -copysignf(1.0f, mat[0]);
	mat[5] = -copysignf(1.0f, mat[4]);
	mat[8] = 1.0f;
}

static void mat3_identity(float m[9]) {
	static const float identity[9] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
	memcpy(m, identity, sizeof(identity));
}

static void mat3_multiply(float mat[9], const float a[9], const float b[9]) {
	float product[9];

	product[0] = a[0] * b[0] + a[1] * b[3] + a[2] * b[6];
	product[1] = a[0] * b[1] + a[1] * b[4] + a[2] * b[7];
	product[2] = a[0] * b[2] + a[1] * b[5] + a[2] * b[8];

	product[3] = a[3] * b[0] + a[4] * b[3] + a[5] * b[6];
	product[4] = a[3] * b[1] + a[4] * b[4] + a[5] * b[7];
	product[5] = a[3] * b[2] + a[4] * b[5] + a[5] * b[8];

	product[6] = a[6] * b[0] + a[7] * b[3] + a[8] * b[6];
	product[7] = a[6] * b[1] + a[7] * b[4] + a[8] * b[7];
	product[8] = a[6] * b[2] + a[7] * b[5] + a[8] * b[8];

	memcpy(mat, product, sizeof(product));
}

static void mat3_translate(float m[9], float x, float y) {
	float translate[9] = {
		1.0f, 0.0f, x,
		0.0f, 1.0f, y,
		0.0f, 0.0f, 1.0f,
	};
	mat3_multiply(m, m, translate);
}

static void mat3_scale(float m[9], float x, float y) {
	float scale[9] = {
		x, 0.0f, 0.0f,
		0.0f, y, 0.0f,
		0.0f, 0.0f, 1.0f,
	};
	mat3_multiply(m, m, scale);
}

bool shady_gl_pipeline_init(struct shady_gl_pipeline *pipeline,
		struct wlr_renderer *renderer) {
	memset(pipeline, 0, sizeof(*pipeline));

	if (!wlr_renderer_is_gles2(renderer)) {
		wlr_log(WLR_ERROR, "gl_pipeline requires GLES2 renderer");
		return false;
	}
	if (!make_egl_current(renderer)) {
		return false;
	}

	char *vert = read_shader_file("window.vert");
	char *frag = read_shader_file("window.frag");
	char *frag_ext = read_shader_file("window_ext.frag");
	if (!vert || !frag || !frag_ext) {
		free(vert);
		free(frag);
		free(frag_ext);
		return false;
	}

	pipeline->prog_2d = link_program(vert, frag, "window.frag");
	pipeline->prog_ext = link_program(vert, frag_ext, "window_ext.frag");
	free(vert);
	free(frag);
	free(frag_ext);

	if (!pipeline->prog_2d || !pipeline->prog_ext) {
		shady_gl_pipeline_fini(pipeline);
		return false;
	}

	pipeline->u_proj_2d = glGetUniformLocation(pipeline->prog_2d, "u_proj");
	pipeline->u_tex_2d = glGetUniformLocation(pipeline->prog_2d, "u_tex");
	pipeline->u_tint_2d = glGetUniformLocation(pipeline->prog_2d, "u_tint");
	pipeline->u_has_alpha_2d = glGetUniformLocation(pipeline->prog_2d, "u_has_alpha");
	pipeline->u_proj_ext = glGetUniformLocation(pipeline->prog_ext, "u_proj");
	pipeline->u_tex_ext = glGetUniformLocation(pipeline->prog_ext, "u_tex");
	pipeline->u_tint_ext = glGetUniformLocation(pipeline->prog_ext, "u_tint");
	pipeline->u_has_alpha_ext = glGetUniformLocation(pipeline->prog_ext, "u_has_alpha");

	wlr_log(WLR_INFO, "GLES2 window pipeline ready (shaders from %s)",
		SHADY_SHADER_DIR);
	return true;
}

void shady_gl_pipeline_fini(struct shady_gl_pipeline *pipeline) {
	if (pipeline->prog_2d) {
		glDeleteProgram(pipeline->prog_2d);
		pipeline->prog_2d = 0;
	}
	if (pipeline->prog_ext) {
		glDeleteProgram(pipeline->prog_ext);
		pipeline->prog_ext = 0;
	}
}

void shady_gl_pipeline_draw_window(struct shady_gl_pipeline *pipeline,
		GLenum target, GLuint tex, bool has_alpha,
		float x, float y, float width, float height,
		int buffer_width, int buffer_height) {
	bool external = (target == GL_TEXTURE_EXTERNAL_OES);
	GLuint prog = external ? pipeline->prog_ext : pipeline->prog_2d;
	GLint u_proj = external ? pipeline->u_proj_ext : pipeline->u_proj_2d;
	GLint u_tex = external ? pipeline->u_tex_ext : pipeline->u_tex_2d;
	GLint u_tint = external ? pipeline->u_tint_ext : pipeline->u_tint_2d;
	GLint u_has_alpha = external ? pipeline->u_has_alpha_ext
		: pipeline->u_has_alpha_2d;

	float proj[9];
	projection_flipped_180(proj, buffer_width, buffer_height);

	/* Same as wlroots set_proj_matrix: translate+scale the unit quad into place. */
	float box[9];
	mat3_identity(box);
	mat3_translate(box, x, y);
	mat3_scale(box, width, height);
	mat3_multiply(box, proj, box);

	glUseProgram(prog);
	glUniformMatrix3fv(u_proj, 1, GL_FALSE, box);
	glUniform4fv(u_tint, 1, WINDOW_TINT);
	glUniform1f(u_has_alpha, has_alpha ? 1.0f : 0.0f);
	glUniform1i(u_tex, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(target, tex);
	glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	if (has_alpha) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glDisable(GL_BLEND);
	}

	/* Client-side verts like wlroots (avoids leftover VBO/VAO state). */
	static const GLfloat verts[] = {
		0.f, 0.f,
		1.f, 0.f,
		0.f, 1.f,
		1.f, 1.f,
	};
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glDisableVertexAttribArray(0);

	glBindTexture(target, 0);
	glUseProgram(0);
}
