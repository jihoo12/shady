#ifndef SHADY_GL_PIPELINE_H
#define SHADY_GL_PIPELINE_H

#include <GLES2/gl2.h>
#include <stdbool.h>

struct wlr_renderer;

struct shady_gl_pipeline {
	GLuint prog_2d;
	GLuint prog_ext;
	GLint u_proj_2d;
	GLint u_tex_2d;
	GLint u_tint_2d;
	GLint u_has_alpha_2d;
	GLint u_proj_ext;
	GLint u_tex_ext;
	GLint u_tint_ext;
	GLint u_has_alpha_ext;
};

bool shady_gl_pipeline_init(struct shady_gl_pipeline *pipeline,
	struct wlr_renderer *renderer);
void shady_gl_pipeline_fini(struct shady_gl_pipeline *pipeline);

/*
 * Draw a textured window quad. x/y/width/height are in output-buffer pixel
 * coordinates (same space wlroots uses inside a render pass).
 */
void shady_gl_pipeline_draw_window(struct shady_gl_pipeline *pipeline,
	GLenum target, GLuint tex, bool has_alpha,
	float x, float y, float width, float height,
	int buffer_width, int buffer_height);

#endif
