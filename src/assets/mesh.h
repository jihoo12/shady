#ifndef SHADY_ASSETS_MESH_H
#define SHADY_ASSETS_MESH_H

#include <stddef.h>
#include <stdbool.h>

struct shady_mesh_vertex {
	float position[3];
	float normal[3];
	float uv[2];
};

/* CPU-side interchange format for asset loaders.
 * Keep this plain C data so loaders implemented in C, C++ or Rust can target
 * the same boundary without exposing renderer internals. */
struct shady_mesh {
	struct shady_mesh_vertex *vertices;
	size_t vertex_count;
	size_t vertex_capacity;
	float bounds_min[3];
	float bounds_max[3];
	bool has_normals;
	bool has_uvs;
};

void shady_mesh_init(struct shady_mesh *mesh);
void shady_mesh_fini(struct shady_mesh *mesh);
bool shady_mesh_reserve(struct shady_mesh *mesh, size_t vertex_capacity);
bool shady_mesh_push_vertex(struct shady_mesh *mesh,
	const struct shady_mesh_vertex *vertex);
void shady_mesh_recalculate_bounds(struct shady_mesh *mesh);

#endif
