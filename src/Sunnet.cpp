#include <iostream>
#include "Sunnet.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

//单例
Sunnet* Sunnet::inst;
Sunnet::Sunnet(){
	inst = this;
}

//开启系统
void Sunnet::Start(){
	std::cout << "Hello Sunnet" << std::endl;
	//忽略SIGPIPE信号
	signal(SIGPIPE, SIG_IGN); //Linux系统的一个坑是对“收到复位（RST）信号的套接字”调用write时，操作系统会向进程发送SIGPIPE信号，默认处理动作是终止进程。因此需要加这句忽略SIGPIPE信号。
	//初始化锁
	assert(pthread_rwlock_init(&servicesLock, NULL) == 0);	
	assert(pthread_spin_init(&globalLock, PTHREAD_PROCESS_PRIVATE) == 0);	
	assert(pthread_cond_init(&sleepCond, NULL) == 0);
	assert(pthread_mutex_init(&sleepMtx, NULL) == 0);
	assert(pthread_rwlock_init(&connsLock, NULL) == 0);
	//開啓Monitor
	StartMonitor();
	//开启Worker
	StartWorker();
	//开启Socket
	StartSocket();
	//开启Timer
	StartTimer();
}

//开启Socket线程
void Sunnet::StartSocket(){
	//创建线程对象
	socketWorker = new SocketWorker();
	//初始化
	socketWorker->Init();
	//创建线程
	socketThread = new std::thread(*socketWorker);
}

//開啓Monitor現場
void Sunnet::StartMonitor(){
	//创建线程对象
	monitor = new Monitor();
	//初始化
	monitor->_count = WORKER_NUM;
	//创建线程
	monitorThread = new std::thread(*monitor);
}

//开启worker线程
void Sunnet::StartWorker(){
	for (int i = 0; i < WORKER_NUM; ++i) {
		std::cout << "start worker thread:" << i << std::endl;
		//创建线程对象
		Worker* worker = new Worker();
		worker->id = i;
		worker->eachNum = 2 << i; //不同的worker线程的eachNum按指数增大，让低编号的工作线程更关注延迟性，高编号的工作线程更关注效率，达到总体平衡
		//監視worker綫程
		NewWorkerMonitor(worker->id);
		//创建线程
		std::thread *wt = new std::thread(*worker);
		//添加到数组
		workers.push_back(worker);
		workerThreads.push_back(wt);
	}
}

//开启Timer线程
void Sunnet::StartTimer(){
	//创建线程对象
	timer = new Timer();
	//初始化
	timer->Init();
	//创建线程
	timerThread = new std::thread(*timer);
}

//等待
void Sunnet::Wait(){
	if(workerThreads[0]) {
		workerThreads[0]->join();	//使调用方阻塞，直到线程退出
	}
}

//新建服务
uint32_t Sunnet::NewService(std::shared_ptr<std::string> type) {
	auto srv = std::make_shared<Service>();
	srv->type = type;
	pthread_rwlock_wrlock(&servicesLock);
	{
		srv->id = maxId;
		maxId++;
		services.emplace(srv->id, srv);
	}
	pthread_rwlock_unlock(&servicesLock);
	srv->OnInit();	//初始化
	return srv->id;
}

void Sunnet::OnServiceErr(uint32_t id){
	std::shared_ptr<Service> srv =GetService(id);
	if(!srv) 
		return;
	srv->OnServiceErr();
}

//由id查找服务
std::shared_ptr<Service> Sunnet::GetService(uint32_t id) {
	std::shared_ptr<Service> srv = NULL;
	pthread_rwlock_rdlock(&servicesLock);
	{
		std::unordered_map<uint32_t, std::shared_ptr<Service>>::iterator iter = services.find(id); //哈希表查找操作时间复杂度为O(1)
		if (iter != services.end()) {
			srv = iter->second;
		}
	}
	pthread_rwlock_unlock(&servicesLock);
	return srv;
}

//删除服务
//只能Service自己调用自己，因为会调用不加锁的srv->OnExit和srv->isExiting
void Sunnet::KillService(uint32_t id) {
	std::shared_ptr<Service> srv = GetService(id);
	if (!srv)
		return;
	//退出前
	srv->OnExit();
	srv->isExiting = true;
	//删除前
	pthread_rwlock_wrlock(&servicesLock);
	{
		services.erase(id);
	}
	pthread_rwlock_unlock(&servicesLock);	
}

//弹出全局队列
std::shared_ptr<Service> Sunnet::PopGlobalQueue() {
	std::shared_ptr<Service> srv = NULL;
	pthread_spin_lock(&globalLock);
	{
		if (!globalQueue.empty()) {
			srv = globalQueue.front();
			globalQueue.pop();
			globalLen--;
		}
	}
	pthread_spin_unlock(&globalLock);
	return srv;	
}

//插入全局队列
void Sunnet::PushGlobalQueue(std::shared_ptr<Service> srv) {
	pthread_spin_lock(&globalLock);
	{
		globalQueue.push(srv);
		globalLen++;
	}	
	pthread_spin_unlock(&globalLock);
}

//发送消息
void Sunnet::Send(uint32_t toId, std::shared_ptr<BaseMsg> msg) {
	std::shared_ptr<Service> toSrv = GetService(toId);
	if (!toSrv) {
		std::cout << "Send fail, toSrv not exist toId:" << toId << std::endl;
		return;
	}
	//插入目标服务器的消息队列
	toSrv->PushMsg(msg);
	//检查并放入全局队列
	bool hasPush = false;
	pthread_spin_lock(&toSrv->inGlobalLock);
	{
		if (!toSrv->inGlobal) {
			PushGlobalQueue(toSrv);
			toSrv->inGlobal = true;
			hasPush = true;
		}
	}
	pthread_spin_unlock(&toSrv->inGlobalLock);
	//唤醒进程
	if(hasPush) {
		CheckAndWeekUp();
	}
}

std::shared_ptr<BaseMsg> Sunnet::MakeBaseMsg(int type) {
	auto msg = std::make_shared<BaseMsg>();
	msg->type = type;
	return msg;
}

std::shared_ptr<BaseMsg> Sunnet::MakeMsg(uint32_t source, char* buff, int len) {
	auto msg = std::make_shared<ServiceMsg>();
	msg->type = BaseMsg::TYPE::SERVICE;
	msg->source = source;
	//基本类型的对象没有析构函数
	//所以用delete或delete[]都可以销毁基本类型数组；
	//智能指针默认使用delete销毁对象
	//所以无须重写智能指针的销毁方法
	msg->buff = std::shared_ptr<char>(buff);
	msg->size = len;
	return msg;
}

std::shared_ptr<BaseMsg> Sunnet::MakeCallbackMsg(uint32_t source, char* buff, int len) {
	auto msg = std::make_shared<ServiceMsg>();
	msg->type = BaseMsg::TYPE::SERVICE_CALLBACK;
	msg->source = source;
	//基本类型的对象没有析构函数
	//所以用delete或delete[]都可以销毁基本类型数组；
	//智能指针默认使用delete销毁对象
	//所以无须重写智能指针的销毁方法
	msg->buff = std::shared_ptr<char>(buff);
	msg->size = len;
	return msg;
}

//Worker线程调用，进入休眠
void Sunnet::WorkerWait() {
	pthread_mutex_lock(&sleepMtx);
	sleepCount++;
	pthread_cond_wait(&sleepCond, &sleepMtx);
	sleepCount--;
	pthread_mutex_unlock(&sleepMtx);
}

//唤醒工作线程
void Sunnet::CheckAndWeekUp() {
	//unsafe
	if(sleepCount == 0) {
		return;
	}
	if(WORKER_NUM - sleepCount <= globalLen) {
		std::cout << "weakup" << std::endl; 	
		pthread_cond_signal(&sleepCond);
	}
}

//工作中的worker綫程數量
int Sunnet::GetWorkingThreadNum() {
	return WORKER_NUM - sleepCount;
}

//添加Conn
int Sunnet::AddConn(int fd, uint32_t id, Conn::TYPE type) {
	auto conn = std::make_shared<Conn>();
	conn->fd = fd;
	conn->serviceId = id;
	conn->type = type;
	pthread_rwlock_wrlock(&connsLock);
	{
		conns.emplace(fd, conn);	
	}
	pthread_rwlock_unlock(&connsLock);
	return fd;
}

//查找Conn
std::shared_ptr<Conn> Sunnet::GetConn(int fd) {
	std::shared_ptr<Conn> conn = NULL;
	pthread_rwlock_rdlock(&connsLock);
	{
		std::unordered_map<uint32_t, std::shared_ptr<Conn>>::iterator iter = conns.find(fd);
		if(iter != conns.end()) {
			conn = iter->second;
		}
	}
	pthread_rwlock_unlock(&connsLock);
	return conn;
}

//删除Conn
bool Sunnet::RemoveConn(int fd) {
	int result = 0;
	pthread_rwlock_wrlock(&connsLock);
	{
		result = conns.erase(fd);
	}
	pthread_rwlock_unlock(&connsLock);
	return result == 1;
}

//开始监听
int Sunnet::Listen(uint32_t port, uint32_t serviceId) {
	//步骤1：创建socket
	int listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenFd <= 0) {
		std::cout << "listen error, listenFd <= 0" << std::endl;
	}
	//步骤2：设置为非阻塞
	fcntl(listenFd, F_SETFL, O_NONBLOCK); //设置为非阻塞后，调用read、write等API接口时，如果socket缓冲区不够，则立即返回不会阻塞等待。
	//步骤3：bind
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int r = bind(listenFd, (struct sockaddr*)&addr, sizeof(addr));
	if(r == -1) {
		std::cout << "listen error, bind fail" << std::endl;
		return -1;
	}
	//步骤4：listen
	r = listen(listenFd, 64); //第二个参数代表“未完成队列的大小”，由于TCP三次握手需要时间，这个数值代表可以容纳的同时进行3次握手的连接数。
	if(r < 0) {
		return -1;
	}
	//步骤5：添加到管理结构
	AddConn(listenFd, serviceId, Conn::TYPE::LISTEN);
	//步骤6：epoll事件，跨线程
	socketWorker->AddEvent(listenFd);
	return listenFd;
}

//关闭连接
void Sunnet::CloseConn(uint32_t fd) {
	//删除conn对象
	bool succ = RemoveConn(fd);
	//关闭套接字
	close(fd);
	//删除epoll对象对套接字的监听（跨线程）
	if(succ) {
		socketWorker->RemoveEvent(fd);
	}
}

void Sunnet::ModifyEvent(int fd, bool epollOut) {
    socketWorker->ModifyEvent(fd, epollOut);
}

int Sunnet::AddTimer(uint32_t serviceId, uint32_t expire, char* cb){
	return timer->AddTimer(serviceId, expire, cb);
}

bool Sunnet::DelTimer(int id){
	return timer->DelTimer(id);
}

void Sunnet::ExpireTimer(){
	timer->ExpireTimer();
}

uint32_t Sunnet::GetNearestTimer(){
	return timer->GetNearestTimer();
}

void Sunnet::NewWorkerMonitor(uint32_t worker_id){
	monitor->NewWorkerMonitor(worker_id);
}

void Sunnet::DeleteWorkerMonitor(uint32_t worker_id){
	monitor->DeleteWorkerMonitor(worker_id);
}

void Sunnet::MonitorTrigger(uint32_t worker_id, int service_id){
	monitor->MonitorTrigger(worker_id, service_id);
}

void Sunnet::MonitorCheck(){
	monitor->MonitorCheck();
}

