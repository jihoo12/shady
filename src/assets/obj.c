#include "obj.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/util/log.h>

struct vec2 { float x, y; };
struct vec3 { float x, y, z; };
struct obj_ref { int v, vt, vn; };

static bool grow(void **data, size_t *capacity, size_t count, size_t size) {
	if (count < *capacity) return true;
	size_t next = *capacity ? *capacity * 2 : 256;
	if (next <= count) next = count + 1;
	if (next > SIZE_MAX / size) return false;
	void *p = realloc(*data, next * size);
	if (!p) return false;
	*data = p;
	*capacity = next;
	return true;
}

static int resolve_index(int index, size_t count) {
	if (index > 0) return index <= (int)count ? index - 1 : -1;
	if (index < 0) {
		long n = (long)count + index;
		return n >= 0 && n < (long)count ? (int)n : -1;
	}
	return -1;
}

static bool parse_ref(const char *s, struct obj_ref *out) {
	char *end;
	errno = 0;
	long v = strtol(s, &end, 10);
	if (errno || end == s || v < INT_MIN || v > INT_MAX) return false;
	*out = (struct obj_ref){ .v = (int)v };
	if (*end != '/') return *end == '\0';
	const char *p = end + 1;
	if (*p != '/') {
		long vt = strtol(p, &end, 10);
		if (end == p || vt < INT_MIN || vt > INT_MAX) return false;
		out->vt = (int)vt;
		p = end;
	}
	if (*p != '/') return *p == '\0';
	p++;
	if (!*p) return true;
	long vn = strtol(p, &end, 10);
	if (end == p || vn < INT_MIN || vn > INT_MAX || *end) return false;
	out->vn = (int)vn;
	return true;
}

static struct vec3 face_normal(struct vec3 a, struct vec3 b, struct vec3 c) {
	float ux=b.x-a.x, uy=b.y-a.y, uz=b.z-a.z;
	float vx=c.x-a.x, vy=c.y-a.y, vz=c.z-a.z;
	struct vec3 n={uy*vz-uz*vy, uz*vx-ux*vz, ux*vy-uy*vx};
	float l=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z);
	if (l > 1e-8f) { n.x/=l; n.y/=l; n.z/=l; }
	return n;
}

static bool emit_triangle(struct shady_mesh *mesh, const struct obj_ref r[3],
		const struct vec3 *pos, size_t pos_n,
		const struct vec2 *uv, size_t uv_n,
		const struct vec3 *normal, size_t normal_n,
		bool *saw_uv, bool *saw_normal) {
	int pi[3], ti[3], ni[3];
	for (int i=0;i<3;i++) {
		pi[i]=resolve_index(r[i].v,pos_n);
		ti[i]=r[i].vt ? resolve_index(r[i].vt,uv_n) : -1;
		ni[i]=r[i].vn ? resolve_index(r[i].vn,normal_n) : -1;
		if (pi[i] < 0 || (r[i].vt && ti[i] < 0) || (r[i].vn && ni[i] < 0))
			return false;
	}
	struct vec3 generated=face_normal(pos[pi[0]],pos[pi[1]],pos[pi[2]]);
	for (int i=0;i<3;i++) {
		struct vec3 n = ni[i] >= 0 ? normal[ni[i]] : generated;
		struct vec2 t = ti[i] >= 0 ? uv[ti[i]] : (struct vec2){0};
		struct shady_mesh_vertex v = {
			.position={pos[pi[i]].x,pos[pi[i]].y,pos[pi[i]].z},
			.normal={n.x,n.y,n.z},
			.uv={t.x,t.y}
		};
		if (!shady_mesh_push_vertex(mesh,&v)) return false;
		if (ti[i] >= 0) *saw_uv=true;
		if (ni[i] >= 0) *saw_normal=true;
	}
	return true;
}

bool shady_obj_load(const char *path, struct shady_mesh *mesh) {
	FILE *f=fopen(path,"r");
	if (!f) { wlr_log(WLR_ERROR,"obj: cannot open %s: %s",path,strerror(errno)); return false; }

	struct vec3 *pos=NULL,*normal=NULL; struct vec2 *uv=NULL;
	size_t pos_n=0,pos_cap=0,normal_n=0,normal_cap=0,uv_n=0,uv_cap=0;
	char *line=NULL; size_t line_cap=0; ssize_t len; unsigned lineno=0;
	bool ok=true,saw_uv=false,saw_normal=false;
	shady_mesh_init(mesh);

	while ((len=getline(&line,&line_cap,f)) >= 0) {
		(void)len; lineno++;
		char *p=line; while (isspace((unsigned char)*p)) p++;
		if (!*p || *p=='#') continue;
		if (p[0]=='v' && isspace((unsigned char)p[1])) {
			struct vec3 v;
			if (sscanf(p+1,"%f %f %f",&v.x,&v.y,&v.z)!=3 ||
			    !grow((void**)&pos,&pos_cap,pos_n,sizeof(*pos))) { ok=false; break; }
			pos[pos_n++]=v;
		} else if (!strncmp(p,"vt",2) && isspace((unsigned char)p[2])) {
			struct vec2 t;
			if (sscanf(p+2,"%f %f",&t.x,&t.y)!=2 ||
			    !grow((void**)&uv,&uv_cap,uv_n,sizeof(*uv))) { ok=false; break; }
			uv[uv_n++]=t;
		} else if (!strncmp(p,"vn",2) && isspace((unsigned char)p[2])) {
			struct vec3 n;
			if (sscanf(p+2,"%f %f %f",&n.x,&n.y,&n.z)!=3 ||
			    !grow((void**)&normal,&normal_cap,normal_n,sizeof(*normal))) { ok=false; break; }
			normal[normal_n++]=n;
		} else if (p[0]=='f' && isspace((unsigned char)p[1])) {
			struct obj_ref *refs=NULL; size_t n=0,cap=0;
			char *save=NULL,*tok=strtok_r(p+1," \t\r\n",&save);
			while (tok) {
				if (!grow((void**)&refs,&cap,n,sizeof(*refs)) || !parse_ref(tok,&refs[n])) { ok=false; break; }
				n++; tok=strtok_r(NULL," \t\r\n",&save);
			}
			if (ok && n < 3) ok=false;
			for (size_t i=1; ok && i+1<n; i++) {
				struct obj_ref tri[3]={refs[0],refs[i],refs[i+1]};
				ok=emit_triangle(mesh,tri,pos,pos_n,uv,uv_n,normal,normal_n,&saw_uv,&saw_normal);
			}
			free(refs);
			if (!ok) { wlr_log(WLR_ERROR,"obj: invalid face at %s:%u",path,lineno); break; }
		}
	}
	free(line); free(pos); free(uv); free(normal); fclose(f);
	if (!ok || !mesh->vertex_count) {
		if (ok) wlr_log(WLR_ERROR,"obj: %s contains no faces",path);
		shady_mesh_fini(mesh);
		return false;
	}
	mesh->has_uvs=saw_uv;
	/* Normals are always populated; missing OBJ normals use flat triangle normals. */
	mesh->has_normals=true;
	wlr_log(WLR_INFO,"obj: loaded %s (%zu triangles, bounds %.3f %.3f %.3f -> %.3f %.3f %.3f)",
		path,mesh->vertex_count/3,
		mesh->bounds_min[0],mesh->bounds_min[1],mesh->bounds_min[2],
		mesh->bounds_max[0],mesh->bounds_max[1],mesh->bounds_max[2]);
	return true;
}


bool shady_obj_load_colliders(const char *path,
		struct shady_box_collider *colliders, size_t capacity, size_t *count) {
	if (count) *count=0;
	FILE *f=fopen(path,"r");
	if (!f) return false;
	struct vec3 *pos=NULL; size_t pos_n=0,pos_cap=0;
	char *line=NULL; size_t line_cap=0; ssize_t len;
	bool ok=true, active=false, have_bounds=false;
	struct shady_box_collider box={0}; size_t out_n=0;

	#define FLUSH_COLLIDER() do { \
		if (active && have_bounds) { \
			if (out_n >= capacity) { ok=false; } \
			else colliders[out_n++]=box; \
		} \
		have_bounds=false; \
	} while (0)

	while (ok && (len=getline(&line,&line_cap,f)) >= 0) {
		(void)len;
		char *p=line; while (isspace((unsigned char)*p)) p++;
		if (p[0]=='v' && isspace((unsigned char)p[1])) {
			struct vec3 v;
			if (sscanf(p+1,"%f %f %f",&v.x,&v.y,&v.z)!=3 ||
					!grow((void**)&pos,&pos_cap,pos_n,sizeof(*pos))) { ok=false; break; }
			pos[pos_n++]=v;
		} else if ((p[0]=='g'||p[0]=='o') && isspace((unsigned char)p[1])) {
			FLUSH_COLLIDER();
			char name[256]={0};
			if (sscanf(p+1,"%255s",name)==1)
				active=!strncmp(name,"collision_",10);
			else active=false;
		} else if (active && p[0]=='f' && isspace((unsigned char)p[1])) {
			char *save=NULL,*tok=strtok_r(p+1," \t\r\n",&save);
			while (tok) {
				if (*tok=='#') break;
				struct obj_ref r;
				if (!parse_ref(tok,&r)) { ok=false; break; }
				int i=resolve_index(r.v,pos_n);
				if (i<0) { ok=false; break; }
				struct vec3 v=pos[i];
				if (!have_bounds) {
					box=(struct shady_box_collider){v.x,v.x,v.y,v.y,v.z,v.z};
					have_bounds=true;
				} else {
					if(v.x<box.min_x)box.min_x=v.x;if(v.x>box.max_x)box.max_x=v.x;
					if(v.y<box.min_y)box.min_y=v.y;if(v.y>box.max_y)box.max_y=v.y;
					if(v.z<box.min_z)box.min_z=v.z;if(v.z>box.max_z)box.max_z=v.z;
				}
				tok=strtok_r(NULL," \t\r\n",&save);
			}
		}
	}
	if (ok) FLUSH_COLLIDER();
	#undef FLUSH_COLLIDER
	free(line); free(pos); fclose(f);
	if (count) *count=out_n;
	if (ok) wlr_log(WLR_INFO,"obj: loaded %zu collision groups from %s",out_n,path);
	return ok;
}


bool shady_obj_load_collision_triangles(const char *path,
		struct shady_triangle_collider *triangles,size_t capacity,size_t *count){
	if(count)*count=0;
	FILE*f=fopen(path,"r"); if(!f)return false;
	struct vec3*pos=NULL;size_t pos_n=0,pos_cap=0,out_n=0;
	char*line=NULL;size_t line_cap=0;ssize_t len;bool ok=true,active=false;
	while(ok&&(len=getline(&line,&line_cap,f))>=0){
		(void)len;char*p=line;while(isspace((unsigned char)*p))p++;
		if(p[0]=='v'&&isspace((unsigned char)p[1])){
			struct vec3 v;
			if(sscanf(p+1,"%f %f %f",&v.x,&v.y,&v.z)!=3||
			   !grow((void**)&pos,&pos_cap,pos_n,sizeof(*pos))){ok=false;break;}
			pos[pos_n++]=v;
		}else if((p[0]=='g'||p[0]=='o')&&isspace((unsigned char)p[1])){
			char name[256]={0};
			active=sscanf(p+1,"%255s",name)==1&&!strncmp(name,"collision_",10);
		}else if(active&&p[0]=='f'&&isspace((unsigned char)p[1])){
			struct obj_ref*refs=NULL;size_t n=0,cap=0;
			char*save=NULL,*tok=strtok_r(p+1," \t\r\n",&save);
			while(tok){
				if(!grow((void**)&refs,&cap,n,sizeof(*refs))||!parse_ref(tok,&refs[n])){ok=false;break;}
				n++;tok=strtok_r(NULL," \t\r\n",&save);
			}
			for(size_t k=1;ok&&k+1<n;k++){
				int ids[3]={resolve_index(refs[0].v,pos_n),resolve_index(refs[k].v,pos_n),resolve_index(refs[k+1].v,pos_n)};
				if(ids[0]<0||ids[1]<0||ids[2]<0||out_n>=capacity){ok=false;break;}
				struct shady_triangle_collider*t=&triangles[out_n++];
				for(int j=0;j<3;j++){struct vec3 v=pos[ids[j]];t->v[j][0]=v.x;t->v[j][1]=v.y;t->v[j][2]=v.z;}
				for(int a=0;a<3;a++){t->min[a]=t->max[a]=t->v[0][a];for(int j=1;j<3;j++){if(t->v[j][a]<t->min[a])t->min[a]=t->v[j][a];if(t->v[j][a]>t->max[a])t->max[a]=t->v[j][a];}}
			}
			free(refs);
		}
	}
	free(line);free(pos);fclose(f);if(count)*count=out_n;
	if(ok)wlr_log(WLR_INFO,"obj: loaded %zu collision triangles from %s",out_n,path);
	return ok;
}
