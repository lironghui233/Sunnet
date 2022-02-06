#include "LuaAPI.h"
#include "stdint.h"
#include "Sunnet.h"
#include <unistd.h>
#include <string.h>
#include <iostream>

//注册Lua模块
void LuaAPI::Register(lua_State *luaState) {
	static luaL_Reg lualibs[] = {
		//第一个参数代表Lua中方法的名字，第二个参数代表对应于C++方法。
		{"NewService", NewService},
		{"KillService", KillService},
		{"Send", Send},
		
		{"Listen", Listen},
		{"CloseConn", CloseConn},
		{"Write", Write},
		{"AddTimer", AddTimer},
		{"DelTimer", DelTimer},
		{NULL, NULL}
	};
	luaL_newlib(luaState, lualibs); //在栈中创建一张新的表，把参数二的函数注册到表中
	lua_setglobal(luaState, "sunnet"); //将栈顶元素放入全局空间，并重新命名
}

//开启新服务
int LuaAPI::NewService(lua_State *luaState) {
	//参数个数
	int num = lua_gettop(luaState); //获取参数个数
	//参数1：服务类型
	if(lua_isstring(luaState, 1) == 0) { //1:是  0：不是
		lua_pushinteger(luaState, -1);
		return 1;
	}
	size_t len = 0;
	const char *type = lua_tolstring(luaState, 1, &len);
	char *newstr = new char[len+1]; //后面加\0
	newstr[len] = '\0';
	memcpy(newstr, type, len); //将字符串又复制一遍原因是Lua字符串是Lua虚拟机管理的，其带有垃圾回收机制，复制一遍为了防止可能发生的冲突
	auto t = std::make_shared<std::string>(newstr);
	//处理
	uint32_t id = Sunnet::inst->NewService(t);
	//返回值
	lua_pushinteger(luaState, id);
	return 1;
}

//关闭服务
int LuaAPI::KillService(lua_State *luaState) {
	//参数个数
	int num = lua_gettop(luaState);
	if(lua_isinteger(luaState, 1) == 0) {
		return 0;
	}
	int id = lua_tointeger(luaState, 1);
	//处理
	Sunnet::inst->KillService(id);
	//返回值（无）
	return 0;
}

//发送消息
int LuaAPI::Send(lua_State *luaState) {
	//参数个数
	int num = lua_gettop(luaState);
	if(num != 3) {
		std::cout << "Send fail, num err" << std::endl;
		return 0;
	}
	//参数1：我是谁
	if(lua_isinteger(luaState, 1) == 0) {
		std::cout << "Send fail, arg1 err" << std::endl;
		return 0;
	}
	int source = lua_tointeger(luaState, 1);
	//参数2：发送给谁
	if(lua_isinteger(luaState, 2) == 0) {
		std::cout << "Send fail, arg2 err" << std::endl;
		return 0;
	}
	int toId = lua_tointeger(luaState, 2);
	//参数3：发送的内容
	if(lua_isstring(luaState, 3) == 0) {
		std::cout << "Send fail, arg3 err" << std::endl;
		return 0;
	}
	size_t len = 0;
	const char *buff = lua_tolstring(luaState, 3, &len);
	//Lua字符串由虚拟机管理，为避免字符串被释放而导致出错，可用memcpy复制一份buff，再用智能指针管理它。
	char *newstr = new char[len];
	memcpy(newstr, buff, len); 
	//处理
	auto msg = std::make_shared<ServiceMsg>();
	msg->type = BaseMsg::TYPE::SERVICE;
	msg->source = source;
	msg->buff = std::shared_ptr<char>(newstr);
	msg->size = len;
	Sunnet::inst->Send(toId, msg);
	//返回值（无）
	return 0;
}

//开启网络监听
int LuaAPI::Listen(lua_State *luaState){
    //参数个数
    int num = lua_gettop(luaState);
    //参数1：端口
    if(lua_isinteger(luaState, 1) == 0) {
        lua_pushinteger(luaState, -1);
        return 1;
    }
    int port = lua_tointeger(luaState, 1);
    //参数2：服务Id
    if(lua_isinteger(luaState, 2) == 0) {
        lua_pushinteger(luaState, -1);
        return 1;
    }
    int service_id = lua_tointeger(luaState, 2);
    //处理
    int listenFd = Sunnet::inst->Listen(port, service_id);
    //返回值
    lua_pushinteger(luaState, listenFd);
    return 1;
}

//关闭连接
int LuaAPI::CloseConn(lua_State *luaState){
    //参数个数
    int num = lua_gettop(luaState);
    //参数1：fd
    if(lua_isinteger(luaState, 1) == 0) {
        return 0;
    }
    int socketfd = lua_tointeger(luaState, 1);
    //处理
    Sunnet::inst->CloseConn(socketfd);
    //返回值
    //（无）
    return 0;
}

//写套接字
int LuaAPI::Write(lua_State *luaState) {
	//参数个数
	int num = lua_gettop(luaState);
	//参数1：fd
	if(lua_isinteger(luaState, 1) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	int fd = lua_tointeger(luaState, 1);
	//参数2：buff
	if(lua_isstring(luaState, 2) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	size_t len = 0;
	const char *buff = lua_tolstring(luaState, 2, &len);
	//处理
	int r = write(fd, buff, len);
	//返回值
	lua_pushinteger(luaState, r);
	return 1;
}

//添加定时器
int LuaAPI::AddTimer(lua_State *luaState){
	//参数个数
	int num = lua_gettop(luaState);
	//参数1：service_id
	if(lua_isinteger(luaState, 1) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	int service_id = lua_tointeger(luaState, 1);
	//参数2：expire 超时时间
	if(lua_isinteger(luaState, 2) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	int expire = lua_tointeger(luaState, 2);
	//参数3：func_name
	if(lua_isstring(luaState, 3) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	size_t len = 0;
	const char *func_name = lua_tolstring(luaState, 3, &len);
	char *newstr = new char[len+1]; //后面加\0
    newstr[len] = '\0';
    memcpy(newstr, func_name, len); //将字符串又复制一遍原因是Lua字符串是Lua虚拟机管理的，其带有垃圾回收机制，复制一遍为了防止可能发生的冲突
	int id = Sunnet::inst->AddTimer(service_id, expire, newstr);
	//返回值
	lua_pushinteger(luaState, id);
	return 1;
}

//删除定时器
int LuaAPI::DelTimer(lua_State *luaState){
	//参数个数
	int num = lua_gettop(luaState);
	//参数1：timer_id
	if(lua_isinteger(luaState, 1) == 0) {
		lua_pushinteger(luaState, -1);
		return 1;
	}
	int timer_id = lua_tointeger(luaState, 1);
	bool is_ok = Sunnet::inst->DelTimer(timer_id);
	//返回值
	lua_pushinteger(luaState, is_ok);
	return 1;
}