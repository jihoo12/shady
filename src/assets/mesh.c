#include "mesh.h"

#include <float.h>
#include <stdlib.h>

void shady_mesh_init(struct shady_mesh *mesh) {
	*mesh = (struct shady_mesh){0};
	mesh->bounds_min[0] = mesh->bounds_min[1] = mesh->bounds_min[2] = FLT_MAX;
	mesh->bounds_max[0] = mesh->bounds_max[1] = mesh->bounds_max[2] = -FLT_MAX;
}

void shady_mesh_fini(struct shady_mesh *mesh) {
	if (!mesh) return;
	free(mesh->vertices);
	shady_mesh_init(mesh);
}

bool shady_mesh_reserve(struct shady_mesh *mesh, size_t capacity) {
	if (capacity <= mesh->vertex_capacity) return true;
	if (capacity > SIZE_MAX / sizeof(*mesh->vertices)) return false;
	struct shady_mesh_vertex *vertices =
		realloc(mesh->vertices, capacity * sizeof(*vertices));
	if (!vertices) return false;
	mesh->vertices = vertices;
	mesh->vertex_capacity = capacity;
	return true;
}

bool shady_mesh_push_vertex(struct shady_mesh *mesh,
		const struct shady_mesh_vertex *vertex) {
	if (mesh->vertex_count == mesh->vertex_capacity) {
		size_t next = mesh->vertex_capacity ? mesh->vertex_capacity * 2 : 256;
		if (next < mesh->vertex_capacity || !shady_mesh_reserve(mesh, next))
			return false;
	}
	mesh->vertices[mesh->vertex_count++] = *vertex;
	for (int i = 0; i < 3; ++i) {
		if (vertex->position[i] < mesh->bounds_min[i])
			mesh->bounds_min[i] = vertex->position[i];
		if (vertex->position[i] > mesh->bounds_max[i])
			mesh->bounds_max[i] = vertex->position[i];
	}
	return true;
}

void shady_mesh_recalculate_bounds(struct shady_mesh *mesh) {
	mesh->bounds_min[0] = mesh->bounds_min[1] = mesh->bounds_min[2] = FLT_MAX;
	mesh->bounds_max[0] = mesh->bounds_max[1] = mesh->bounds_max[2] = -FLT_MAX;
	for (size_t n = 0; n < mesh->vertex_count; ++n) {
		for (int i = 0; i < 3; ++i) {
			float v = mesh->vertices[n].position[i];
			if (v < mesh->bounds_min[i]) mesh->bounds_min[i] = v;
			if (v > mesh->bounds_max[i]) mesh->bounds_max[i] = v;
		}
	}
	if (!mesh->vertex_count) {
		for (int i = 0; i < 3; ++i)
			mesh->bounds_min[i] = mesh->bounds_max[i] = 0.0f;
	}
}
