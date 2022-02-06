#pragma once
#include <queue>
#include <unordered_map>
#include <thread>
#include "Msg.h"
#include "ConnWriter.h"

//因为是Lua是由C语言编写，因此包含Lua源码中头文件时，需要把include语句放在"extern "C""块中
extern "C" {
	//lua.h仅包含声明。如果不把lua.c加入编译，编译器就要寻求第二种方案，即找到liblua.a。复制其具体实现
	//".a"文件称为静态链接库，其中存储着多个".c"和".cpp"文件编译后的二进制代码。".o"是目标文件，".a"由多个".o"文件组合而成
	//".so"文件称为动态链接库
	#include "lua.h"
	#include "lauxlib.h"
	#include "lualib.h"
}

class Service {
public:
	//服务编号id，每个服务的id是唯一的
	uint32_t id;
	//服务类型
	std::shared_ptr<std::string> type;
	//是否正在退出
	bool isExiting = false;
	//消息列表和自旋锁
	std::queue<std::shared_ptr<BaseMsg>> msgQueue;
	pthread_spinlock_t queueLock;
public:
	//构造和析构函数
	Service();
	~Service();	
	//回调函数
	void OnInit();
	void OnMsg(std::shared_ptr<BaseMsg> msg);
	void OnExit();
	//插入消息
	void PushMsg(std::shared_ptr<BaseMsg> msg);
	//执行消息
	bool ProcessMsg();
	void ProcessMsgs(int max);
private:
	//取出一条消息
	std::shared_ptr<BaseMsg> PopMsg();
public:
	//标记是否在全局队列，true：表示在全局队列中，或正在处理
	bool inGlobal = false;
	pthread_spinlock_t inGlobalLock;
	//线程安全地设置inGlobal
	void SetInGlobal(bool isIn);
private:
	//消息处理方法
	void OnServiceMsg(std::shared_ptr<ServiceMsg> msg);	
	void OnServiceCallbackMsg(std::shared_ptr<ServiceMsg> msg);	
	void OnAcceptMsg(std::shared_ptr<SocketAcceptMsg> msg);	
	void OnRWMsg(std::shared_ptr<SocketRWMsg> msg);	
	void OnSocketData(int fd, const char* buff, int len);	
	void OnSocketWritable(int fd);	
	void OnSocketClose(int fd);	
public:	
	void OnServiceErr();	
public:
	//写缓冲区 （测试用）
	std::unordered_map<int, std::shared_ptr<ConnWriter>> writers;	//<socketfd, ConnWriter>
private:
	//Lua虚拟机
	lua_State *luaState;	
};
