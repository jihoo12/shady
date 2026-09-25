#ifndef SHADY_MODULE_LUA_H
#define SHADY_MODULE_LUA_H
#include <stdbool.h>
struct shady_server;
bool shady_lua_init(struct shady_server *server);
void shady_lua_fini(struct shady_server *server);
#endif
