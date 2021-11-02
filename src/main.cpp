#include "Sunnet.h"
#include <unistd.h>
#include <assert.h>

int testPingPong() {

	auto pingType = std::make_shared<std::string>("ping");

	uint32_t ping1 = Sunnet::inst->NewService(pingType);
	uint32_t ping2 = Sunnet::inst->NewService(pingType);
	uint32_t pong = Sunnet::inst->NewService(pingType);

	auto msg1 = Sunnet::inst->MakeMsg(ping1, new char[3]{'h','i','\0'}, 3);
	auto msg2 = Sunnet::inst->MakeMsg(ping2, new char[6]{'h','e','l','l','o','\0'}, 6);

	Sunnet::inst->Send(pong, msg1);
	Sunnet::inst->Send(pong, msg2);
}

int testSocketCtrl() {
	int fd = Sunnet::inst->Listen(8001, 1);
	usleep(15*10000000);
	Sunnet::inst->CloseConn(fd);
}

int testEcho() {
	auto t = std::make_shared<std::string>("gateway");
	uint32_t gateway = Sunnet::inst->NewService(t);
}

int main(){

	//创建守护进程。让程序转入后台运行，就算断开终端（SSH会话）也不会中断程序。因为该进程忽略了SIGHUP信号。
	//daemon(0, 0);
	
	//创建Sunnet引擎
	new Sunnet();
	//开始引擎
	Sunnet::inst->Start();

	//test
	//testPingPong();	
	//testSocketCtrl();
	//testEcho();

	//启动main服务
	auto t = std::make_shared<std::string>("main");
	Sunnet::inst->NewService(t);

	Sunnet::inst->Wait();
	return 0;
}
