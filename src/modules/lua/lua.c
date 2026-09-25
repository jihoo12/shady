#include "lua.h"
#include <stdbool.h>
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <wlr/util/log.h>
#include "../../shady.h"

static int l_shady_log(lua_State *L){
	const char *message=luaL_checkstring(L,1);
	wlr_log(WLR_INFO,"[SHADY LUA] 🌙 %s",message);
	return 0;
}
static void install_api(lua_State *L){
	lua_newtable(L);
	lua_pushcfunction(L,l_shady_log);lua_setfield(L,-2,"log");
	lua_setglobal(L,"shady");
}
static void default_script_path(char *buf,size_t size){
	const char *xdg=getenv("XDG_CONFIG_HOME"),*home=getenv("HOME");
	if(xdg&&*xdg)snprintf(buf,size,"%s/shady/init.lua",xdg);
	else if(home&&*home)snprintf(buf,size,"%s/.config/shady/init.lua",home);
	else snprintf(buf,size,"init.lua");
}
bool shady_lua_init(struct shady_server *server){
	lua_State *L=luaL_newstate();if(!L){wlr_log(WLR_ERROR,"[SHADY LUA] failed to create Lua state");return false;}
	server->lua.L=L;luaL_openlibs(L);install_api(L);
	char path[4096];default_script_path(path,sizeof(path));
	if(luaL_loadfile(L,path)!=LUA_OK){
		const char *e=lua_tostring(L,-1);
		if(e&&strstr(e,"No such file or directory"))wlr_log(WLR_INFO,"[SHADY LUA] 💤 no init script: %s",path);
		else wlr_log(WLR_ERROR,"[SHADY LUA] load error: %s",e?e:"unknown error");
		lua_pop(L,1);return true;
	}
	if(lua_pcall(L,0,0,0)!=LUA_OK){wlr_log(WLR_ERROR,"[SHADY LUA] runtime error: %s",lua_tostring(L,-1));lua_pop(L,1);return true;}
	wlr_log(WLR_INFO,"[SHADY LUA] ✅ loaded %s (Lua %s)",path,LUA_VERSION);
	return true;
}
void shady_lua_fini(struct shady_server *server){
	if(server->lua.L){lua_close(server->lua.L);server->lua.L=NULL;}
}
