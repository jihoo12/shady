#ifndef SHADY_GL_PIPELINE_H
#define SHADY_GL_PIPELINE_H

#include <GLES2/gl2.h>
#include <stdbool.h>

struct wlr_renderer;

struct shady_gl_pipeline {
	GLuint prog_2d;
	GLuint prog_ext;

	GLint u_mvp_2d;
	GLint u_tex_2d;
	GLint u_tint_2d;
	GLint u_has_alpha_2d;
	GLint u_time_2d;
	GLint u_wobble_2d;
	GLint u_close_progress_2d;

	GLint u_mvp_ext;
	GLint u_tex_ext;
	GLint u_tint_ext;
	GLint u_has_alpha_ext;
	GLint u_time_ext;
	GLint u_wobble_ext;
	GLint u_close_progress_ext;

	/*
	 * Subdivided mesh used for wobbly and crumple deformation.
	 */
	GLuint mesh_vbo;
	GLsizei mesh_vertex_count;
};

bool shady_gl_pipeline_init(
	struct shady_gl_pipeline *pipeline,
	struct wlr_renderer *renderer
);

void shady_gl_pipeline_fini(
	struct shady_gl_pipeline *pipeline
);

void shady_gl_pipeline_draw_window(
	struct shady_gl_pipeline *pipeline,
	GLenum target,
	GLuint tex,
	bool has_alpha,
	const float mvp[16],
	float time_seconds,
	float wobble_x,
	float wobble_y,
	float close_progress
);

#endif