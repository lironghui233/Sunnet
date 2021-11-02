#include "Service.h"
#include "Sunnet.h"
#include <iostream>
#include <unistd.h>
#include <string.h>
#include "LuaAPI.h"

//构造函数
Service::Service() {
	//初始化锁
	pthread_spin_init(&queueLock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&inGlobalLock, PTHREAD_PROCESS_PRIVATE);
}

//析构函数
Service::~Service() {
	//释放锁
	pthread_spin_destroy(&queueLock);
	pthread_spin_destroy(&inGlobalLock);
}

//插入消息
void Service::PushMsg(std::shared_ptr<BaseMsg> msg) {
	pthread_spin_lock(&queueLock);
	{
		msgQueue.push(msg);
	}	
	pthread_spin_unlock(&queueLock);
}

//取出消息
std::shared_ptr<BaseMsg> Service::PopMsg() {
	std::shared_ptr<BaseMsg> msg = NULL;
	//取出一条消息
	pthread_spin_lock(&queueLock);
	{
		if (!msgQueue.empty()) {
			msg = msgQueue.front();
			msgQueue.pop();
		}
	}
	pthread_spin_unlock(&queueLock);
	return msg;
}

//创建服务后触发
void Service::OnInit() {
	std::cout << "[" << id << "] OnInit" << std::endl;
	//新建Lua虚拟机
	luaState = luaL_newstate();
	//开启全部标准库
	luaL_openlibs(luaState);
	//注册Sunnet系统API
	LuaAPI::Register(luaState);
	//执行Lua文件
	std::string filename = "../service/" + *type + "/init.lua";
	int isok = luaL_dofile(luaState, filename.data());
	if(isok == 1) { //若成功则返回值未0，若失败则返回值为1
		std::cout << "run lua fail:" << lua_tostring(luaState, -1) << std::endl;
	}
	//调用Lua函数
	lua_getglobal(luaState, "OnInit"); //把指定全局变量压栈，并返回该值的类型
	lua_pushinteger(luaState, id); //把整型数压栈
	isok = lua_pcall(luaState, 1, 0, 0); //调用一个Lua方法。参数二代表Lua方法的参数值个数，参数三代表Lua方法的返回值个数，参数四代表如果调用失败应该采取什么样的处理方法，填写0代表使用默认方式
	if(isok != 0) {	//若返回值为0则代表成功，否则代表失败
		std::cout << "call lua OnInit fail " << lua_tostring(luaState, -1) << std::endl;
	} 
}

//收到消息时触发
void Service::OnMsg(std::shared_ptr<BaseMsg> msg) {
	//std::cout << "[" << id << "] OnMsg" << std::endl;

	if (msg->type == BaseMsg::TYPE::SERVICE) {
		auto m = std::dynamic_pointer_cast<ServiceMsg>(msg);
		OnServiceMsg(m);
	} 
	else if (msg->type == BaseMsg::TYPE::SOCKET_ACCEPT) {
		auto m = std::dynamic_pointer_cast<SocketAcceptMsg>(msg);
		OnAcceptMsg(m);
	}
	else if (msg->type == BaseMsg::TYPE::SOCKET_RW) {
		auto m = std::dynamic_pointer_cast<SocketRWMsg>(msg);
		OnRWMsg(m);
	}
}

//退出服务时触发
void Service::OnExit() {
	std::cout << "[" << id << "] OnExit" << std::endl;
	//调用Lua函数
	lua_getglobal(luaState, "OnExit");
	int isok = lua_pcall(luaState, 0, 0, 0); //C++与Lua是单线程交互，lua_pcall的执行时间即Lua脚本的运行时间。
	if(isok != 0) { //若返回值为0则代表成功，否则代表失败
		std::cout << "call lua OnExit fail " << lua_tostring(luaState, -1) << std::endl;
	}
	//关闭Lua虚拟机
	lua_close(luaState);
}

//处理一条消息，返回值代表是否处理
bool Service::ProcessMsg() {
	std::shared_ptr<BaseMsg> msg = PopMsg();
	if (msg) {
		OnMsg(msg);
		return true;
	} 
	else {
		return false;	//返回值预示着队列是否为空
	} 
}

//处理N条消息，返回值代表是否处理
void Service::ProcessMsgs(int max) {
	for (int i = 0; i < max; ++i) {
		bool succ = ProcessMsg();
		if(!succ) {
			break;
		}
	}
}

//安全地设置inGlobal变量
void Service::SetInGlobal(bool isIn) {
	pthread_spin_lock(&inGlobalLock);
	{
		inGlobal = isIn;
	}
	pthread_spin_unlock(&inGlobalLock);
}


void Service::OnServiceMsg(std::shared_ptr<ServiceMsg> msg) {
	std::cout << " OnServiceMsg " << std::endl;
	//调用Lua函数
	lua_getglobal(luaState, "OnServiceMsg");
	lua_pushinteger(luaState, msg->source);
	lua_pushlstring(luaState, msg->buff.get(), msg->size);
	int isok = lua_pcall(luaState, 2, 0, 0);
	if(isok != 0) { //若返回值为0则代表成功，否者代表失败
		std::cout << "call lua OnServiceMsg fail" << lua_tostring(luaState, -1) << std::endl;
	}	
}	

void Service::OnAcceptMsg(std::shared_ptr<SocketAcceptMsg> msg) {
	std::cout << " OnAcceptMsg " << msg->clientFd << std::endl;
	auto w = std::make_shared<ConnWriter>();
	w->fd = msg->clientFd;
	writers.emplace(msg->clientFd, w);

	//调用Lua函数
	lua_getglobal(luaState, "OnAcceptMsg");
	lua_pushinteger(luaState, msg->listenFd);
	lua_pushinteger(luaState, msg->clientFd);
	int isok = lua_pcall(luaState, 2, 0, 0);
	if(isok != 0) { //若返回值为0则代表成功，否则代表失败
		std::cout << "call lua OnAcceptMsg fail" << lua_tostring(luaState, -1) << std::endl;
	}
}	

void Service::OnRWMsg(std::shared_ptr<SocketRWMsg> msg) {
	std::cout << " OnRWMsg " << std::endl;
	int fd = msg->fd;
	//可读
	if(msg->isRead) {
		const int BUFFSIZE = 512;
		char buff[BUFFSIZE] = {0};
		int len = 0;
		do {
			len = read(fd, &buff, BUFFSIZE);
			if(len > 0) {
				OnSocketData(fd, buff, len);
			}
		} while(len == BUFFSIZE); //两种情况：情况一是套接字的读缓冲区恰好有512字节，一次性读出，然后下一次循环中，read会返回-1，并设置EAGAIN（数据读完），此种情况，程序不会进入读取失败的分支。情况二是读缓冲区大于512，因此一直循环读取直到读完读缓冲区的数据。

		if(len <= 0 && errno != EAGAIN) {
			if(Sunnet::inst->GetConn(fd)) { //断开连接前确认还存在连接conn
				OnSocketClose(fd);
				Sunnet::inst->CloseConn(fd);
			}
		}
	}
	//可写
	if(msg->isWrite) {
		if(Sunnet::inst->GetConn(fd)) {
			OnSocketWritable(fd);
		}
	}
}		

//收到客户端消息
void Service::OnSocketData(int fd, const char* buff, int len) {
	std::cout << "OnSocketData " << fd << " buff: " << buff << std::endl;
	
	//echo
	//char writeBuff[3] = {'l','r','h'};
	//write(fd, writeBuff, 3);

   	//发送大量数据实验
   	//char* wirteBuff = new char[4200000];
   	//wirteBuff[4200000-1] = 'e';
   	//int r = write(fd, wirteBuff, 4200000); 
   	//cout << "write r:" << r <<  " " << strerror(errno) <<  endl;	
	
	//用ConnWriter发送大量数据
	// char* writeBuff = new char[4200000];
	// writeBuff[4200000-1] = 'e';
	// auto w = writers[fd];
	// w->EntireWrite(std::shared_ptr<char>(writeBuff), 4200000);
	// w->LingerClose();

	//调用Lua函数
	lua_getglobal(luaState, "OnSocketData");
	lua_pushinteger(luaState, fd);
	lua_pushlstring(luaState, buff, len);
	int isok = lua_pcall(luaState, 2, 0, 0);
	if(isok != 0) { //若返回值为0则代表成功，否则代表失败
		std::cout << "call lua OnSocketData fail" << lua_tostring(luaState, -1) << std::endl;
	}
}	

//套接字可写
void Service::OnSocketWritable(int fd) {
	std::cout << "OnSocketWritable " << fd << std::endl; 
	auto w = writers[fd];
	w->OnWriteable();
}	

//关闭连接前
void Service::OnSocketClose(int fd) {
	std::cout << "OnSocketClose " << fd << std::endl;
	writers.erase(fd);

	//调用Lua函数
	lua_getglobal(luaState, "OnSocketClose");
	lua_pushinteger(luaState, fd);
	int isok = lua_pcall(luaState, 1, 0, 0);
	if(isok != 0) { //若返回值为0则代表成功，否则代表失败
		std::cout << "call lua OnSocketClose fail" << lua_tostring(luaState, -1) << std::endl;
	}
}	