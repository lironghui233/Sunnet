#include <iostream>
#include <unistd.h>
#include <string.h>
#include "Monitor.h"
#include "Service.h"

#define DEFAULT_SLEEP_TIME 1*1000000 

#define CHECK_ABORT if (Sunnet::inst->GetWorkingThreadNum()==0) return;

//线程函数
void Monitor::operator()() {
	while(true) {
		//每5秒檢測一次
		usleep(5*DEFAULT_SLEEP_TIME);
		Sunnet::inst->MonitorCheck();		
	}
}

void Monitor::MonitorTrigger(uint32_t worker_id, int service_id) {
	std::shared_ptr<WrorkerMonitor> worker_monitor = GetWorkerMonitor(worker_id);
	if (!worker_monitor)
		return;
	worker_monitor->version ++;
	worker_monitor->service_id = service_id;
	// ATOM_FINC(&mt->version);
}

void Monitor::MonitorCheck() {
	CHECK_ABORT
	int worker_id = 0;
	for (worker_id = 0; worker_id < Count(); ++worker_id)
	{
		std::shared_ptr<WrorkerMonitor> worker_monitor = GetWorkerMonitor(worker_id);
		if (!worker_monitor)
			return;
		if (worker_monitor->version == worker_monitor->check_version) {
			if (worker_monitor->service_id) {
				Sunnet::inst->OnServiceErr(worker_monitor->service_id);
			}
		} else {
			worker_monitor->check_version = worker_monitor->version;
		}
	}
}

std::shared_ptr<WrorkerMonitor> Monitor::GetWorkerMonitor(uint32_t worker_id){
	std::shared_ptr<WrorkerMonitor> worker_monitor = NULL;
	std::unordered_map<uint32_t, std::shared_ptr<WrorkerMonitor>>::iterator iter = wrorkerMonitors.find(worker_id); //哈希表查找操作时间复杂度为O(1)
	if (iter != wrorkerMonitors.end()) {
		worker_monitor = iter->second;
	}
	return worker_monitor;
}

void Monitor::NewWorkerMonitor(uint32_t worker_id) {
	auto worker_monitor = std::make_shared<WrorkerMonitor>();
	worker_monitor->version = 0;
	worker_monitor->check_version = 0;
	worker_monitor->service_id = 0;
	wrorkerMonitors.emplace(worker_id, worker_monitor);
}

void Monitor::DeleteWorkerMonitor(uint32_t worker_id) {
	std::shared_ptr<WrorkerMonitor> worker_monitor = GetWorkerMonitor(worker_id);
	if (!worker_monitor)
		return;
	wrorkerMonitors.erase(worker_id);
}
