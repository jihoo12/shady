#include "shady.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>

void shady_config_defaults(struct shady_config *c) {
	*c = (struct shady_config){
		.window_gravity = false,
		.window_wobble = true,
		.window_sides = true,
		.shadows = true,
		.floor = true,
		.close_animation = true,
		.fps_mode = true,
	};
}

static char *trim(char *s) {
	while (isspace((unsigned char)*s)) s++;
	char *end = s + strlen(s);
	while (end > s && isspace((unsigned char)end[-1])) end--;
	*end = '\0';
	return s;
}

static bool parse_bool(const char *s, bool *out) {
	if (!strcasecmp(s,"true") || !strcasecmp(s,"yes") || !strcasecmp(s,"on") || !strcmp(s,"1")) { *out=true; return true; }
	if (!strcasecmp(s,"false") || !strcasecmp(s,"no") || !strcasecmp(s,"off") || !strcmp(s,"0")) { *out=false; return true; }
	return false;
}

bool shady_config_load(struct shady_config *c, const char *path) {
	FILE *fp = fopen(path, "r");
	if (!fp) {
		if (errno != ENOENT) wlr_log(WLR_ERROR, "config: cannot open %s: %s", path, strerror(errno));
		else wlr_log(WLR_INFO, "config: %s not found, using defaults", path);
		return false;
	}
	char line[512]; unsigned lineno=0;
	while (fgets(line, sizeof(line), fp)) {
		lineno++;
		char *p=trim(line);
		if (!*p || *p=='#' || *p==';') continue;
		char *eq=strchr(p,'=');
		if (!eq) { wlr_log(WLR_ERROR,"config:%u: expected key = value",lineno); continue; }
		*eq='\0'; char *key=trim(p), *value=trim(eq+1);
		char *comment=strpbrk(value,"#;");
		if (comment) { *comment='\0'; value=trim(value); }
		bool v;
		if (!parse_bool(value,&v)) { wlr_log(WLR_ERROR,"config:%u: invalid boolean '%s'",lineno,value); continue; }
#define KEY(name, field) if (!strcmp(key,name)) { c->field=v; continue; }
		KEY("window_gravity",window_gravity)
		KEY("window_wobble",window_wobble)
		KEY("window_sides",window_sides)
		KEY("shadows",shadows)
		KEY("floor",floor)
		KEY("close_animation",close_animation)
		KEY("fps_mode",fps_mode)
#undef KEY
		wlr_log(WLR_ERROR,"config:%u: unknown key '%s'",lineno,key);
	}
	fclose(fp);
	wlr_log(WLR_INFO,"config: loaded %s",path);
	return true;
}
