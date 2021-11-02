#include "SocketWorker.h"
#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "Sunnet.h"
#include <fcntl.h>
#include <sys/socket.h>

//初始化
void SocketWorker::Init() {
	std::cout << "SocketWorker Init" << std::endl;
	//创建epoll对象，返回值：非负数表示成功创建的epoll对象的描述符，-1表示创建失败
	epollFd = epoll_create(1024);	//创建epoll对象，epoll对象也是操作系统管理的，返回epoll对象的描述符给该进程的文件描述符
	assert(epollFd > 0);
}

void SocketWorker::operator()() {
	while(true){
		//阻塞等待
		const int EVENT_SIZE = 64;
		struct epoll_event events[EVENT_SIZE];
		int eventCount = epoll_wait(epollFd, events, EVENT_SIZE, -1);
		//取得事件
		for(int i = 0; i < eventCount; ++i) {
			epoll_event ev = events[i];	//当前要处理的事件
			OnEvent(ev);
		}	
	}
}

//新增epoll监听对象，注意跨线程调用
void SocketWorker::AddEvent(int fd) {
	std::cout << "AddEvent fd " << fd << std::endl;
	//添加到epoll对象
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET; //EPOLLIN代表可读，EPOLLOUT代表可写，EPOLLET代表边缘触发
	ev.data.fd = fd;
	//把fd这个socket添加到epollFd对应的epoll对象的监听列表
	if(epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		std::cout << "AddEvent epoll_ctl Fail:" << strerror(errno) << std::endl;
	}
}

//删除epoll监听对象，跨线程调用
void SocketWorker::RemoveEvent(int fd) {
	std::cout << "RemoveEvent fd " << fd << std::endl;
	epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
}

//修改epoll监听对象，跨线程调用
void SocketWorker::ModifyEvent(int fd, bool epollOut) {
	std::cout << "ModifyEvent fd " << fd << " " << epollOut << std::endl;
	struct epoll_event ev;
	ev.data.fd = fd;
	
	if(epollOut) {
		ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
	}
	else {
		ev.events = EPOLLIN | EPOLLET;
	}
	epoll_ctl(epollFd, EPOLL_CTL_MOD, fd, &ev);
}

void SocketWorker::OnEvent(epoll_event ev) {
	//std::cout << "OnEvent" << std::endl;
	int fd = ev.data.fd;
	auto conn = Sunnet::inst->GetConn(fd);
	if(conn == NULL) {
		std::cout << "OnEvent error, conn == NULL" << std::endl;
		return; 
	}
	//事件类型
	bool isRead = ev.events & EPOLLIN;
	bool isWrite = ev.events & EPOLLOUT;
	bool isError = ev.events & EPOLLERR;
	//监听socket
	if(conn->type == Conn::TYPE::LISTEN) {
		if(isRead) {
			OnAccept(conn);
		}
	}
	//普通socket
	else {
		if(isRead || isWrite) {
			OnRW(conn, isRead, isWrite);
		}
		if(isError) {
			std::cout << "OnError fd:" << conn->fd << std::endl;
		}
	}
}

void SocketWorker::OnAccept(std::shared_ptr<Conn> conn) {
	std::cout << "OnAccept fd:" << conn->fd << std::endl;
	//步骤1：accept
	int clientFd = accept(conn->fd, NULL, NULL); //此时操作系统内核会创建一个新的套接字结构，代表该客户端连接，并返回它的文件描述符。
	if(clientFd < 0) {
		std::cout << "accpet error" << std::endl;
	}
	//步骤2：设置非阻塞
	fcntl(clientFd, F_SETFL, O_NONBLOCK);
	//设置写缓冲区大小。注意一般不用修改该值。因为可能存在玩家对应socket写缓冲区占据太多内存会导致服务器内存不足而dump掉
	// unsigned long buffSize = 4294967295; // 4G
	// if(setsockopt(clientFd, SOL_SOCKET, SO_SNDBUFFORCE, &buffSize, sizeof(buffSize)) < 0) {
	// 	std::cout << "OnAccept setsockopt Fail " << strerror(errno) << std::endl;
	// }
	//步骤3：添加连接对象
	Sunnet::inst->AddConn(clientFd, conn->serviceId, Conn::TYPE::CLIENT);
	//步骤4：添加到epoll监听列表
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = clientFd;
	if(epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &ev) == -1) {
		std::cout << "OnAccept epoll_ctrl Fail:" << strerror(errno) << std::endl;
	}
	//步骤5：通知服务
	auto msg = std::make_shared<SocketAcceptMsg>();
	msg->type = BaseMsg::TYPE::SOCKET_ACCEPT;
	msg->listenFd = conn->fd;
	msg->clientFd = clientFd;
	Sunnet::inst->Send(conn->serviceId, msg);
}

void SocketWorker::OnRW(std::shared_ptr<Conn> conn, bool r, bool w) {
	std::cout << "OnRW fd:" << conn->fd << std::endl;
	auto msg = std::make_shared<SocketRWMsg>();
	msg->type = BaseMsg::TYPE::SOCKET_RW;
	msg->fd = conn->fd;
	msg->isRead = r;
	msg->isWrite = w;
	Sunnet::inst->Send(conn->serviceId, msg);
}