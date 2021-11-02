#include <iostream>
#include <unistd.h>
#include "Worker.h"
#include "Service.h"

//线程函数
void Worker::operator()() {
	while(true) {
		std::shared_ptr<Service> srv = Sunnet::inst->PopGlobalQueue();
		if (!srv) {
			Sunnet::inst->WorkerWait();
		}
		else {
			srv->ProcessMsgs(eachNum);
			CheckAndPutGlobal(srv);
		}
	}
}

//判断是否还有未处理的消息，如果有，把Service重新放回全局队列中
void Worker::CheckAndPutGlobal(std::shared_ptr<Service> srv) {
	//退出中（服务的退出方式只能它自己调用，这样isExiting才不会产生线程冲突）
	if (srv->isExiting) {
		return;
	}
	
	pthread_spin_lock(&srv->queueLock);
	{
		//重新放回全局队列
		if (!srv->msgQueue.empty()) {
			//此时srv->inGlobal一定是true
			Sunnet::inst->PushGlobalQueue(srv);
		}
		//不在队列中，重设inGlobal
		else {
			srv->SetInGlobal(false);
		}
	}
	pthread_spin_unlock(&srv->queueLock);
}
