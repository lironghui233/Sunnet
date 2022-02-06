#pragma once

extern "C" {
	#include "lua.h"
}

class LuaAPI {
public:
	static void Register(lua_State *luaState);

	static int NewService(lua_State *luaState);
	static int KillService(lua_State *luaState);
	static int Send(lua_State *luaState);

	static int Listen(lua_State *luaState);
	static int CloseConn(lua_State *luaState);
	static int Write(lua_State *luaState);
	
	static int AddTimer(lua_State *luaState);
	static int DelTimer(lua_State *luaState);
};
