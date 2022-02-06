#include <unistd.h>
#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#include <sys/time.h>
#include <mach/task.h>
#include <mach/mach.h>
#else
#include <time.h>
#endif

#include <iostream>
#include <string.h>

#include "Timer.h"

#define DEFAULT_SLEEP_TIME 1*1000000 

void Timer::Init(){
	std::cout << "Timer Init" << std::endl;
}

//线程函数
void Timer::operator()() {
	while(true) {
        int sleep_time = Sunnet::inst->GetNearestTimer();
 		usleep(sleep_time);
 		Sunnet::inst->ExpireTimer(); // 更新检测定时器，并把定时事件发送到消息队列中
	}
}

// 底层实现 -- 最小堆

static uint32_t
current_time() {
	uint32_t t;
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
	struct timespec ti;
	clock_gettime(CLOCK_MONOTONIC, &ti);
	t = (uint32_t)ti.tv_sec * 1000;
	t += ti.tv_nsec / 1000000;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	t = (uint32_t)tv.tv_sec * 1000;
	t += tv.tv_usec / 1000;
#endif
	return t;
}

int Timer::AddTimer(uint32_t service_id, uint32_t expire, char* cb) {
    int64_t timeout = current_time() + expire;
    TimerNode* node = new TimerNode;
    int id = Count();
    node->id = id;
    node->service_id = service_id;
    node->expire = timeout;
    node->cb = cb;
    node->idx = (int)_heap.size();
    _heap.push_back(node);
    _shiftUp((int)_heap.size() - 1);
    _map.insert(std::make_pair(id, node));
    return id;
}

bool Timer::DelTimer(int id)
{
    auto iter = _map.find(id);
    if (iter == _map.end())
        return false;
    _delNode(iter->second);
    return true;
}

uint32_t Timer::GetNearestTimer()
{
    if (_heap.empty()) 
        return DEFAULT_SLEEP_TIME;
    TimerNode* node = _heap.front();
    uint32_t expire = node->expire;
    uint32_t now = current_time();
    uint32_t sleep_time = expire-now;
    if ( sleep_time < 0 ) 
        return 0;
    if ( sleep_time < DEFAULT_SLEEP_TIME )
        return DEFAULT_SLEEP_TIME;  
    return sleep_time;
}

void Timer::ExpireTimer()
{
    if (_heap.empty()) return;
    uint32_t now = current_time();
    do {
        TimerNode* node = _heap.front();
        if (now < node->expire)
            break;
        auto msg = Sunnet::inst->MakeCallbackMsg(node->service_id, node->cb, strlen(node->cb));   
        Sunnet::inst->Send(node->service_id, msg);
        _delNode(node);
    } while(!_heap.empty());
}

void Timer::_delNode(TimerNode *node)
{
    int last = (int)_heap.size() - 1;
    int idx = node->idx;
    if (idx != last) {
        std::swap(_heap[idx], _heap[last]);
        _heap[idx]->idx = idx;
        if (!_shiftDown(idx)) {
            _shiftUp(idx);
        }
    }
    _heap.pop_back();
    _map.erase(node->id);
    delete node;
}

bool Timer::_shiftDown(int pos){
    int last = (int)_heap.size()-1;
    int idx = pos;
    for (;;) {
        int left = 2 * idx + 1;
        if ((left >= last) || (left < 0)) {
            break;
        }
        int min = left; // left child
        int right = left + 1;
        if (right < last && !_lessThan(left, right)) {
            min = right; // right child
        }
        if (!_lessThan(min, idx)) {
            break;
        }
        std::swap(_heap[idx], _heap[min]);
        _heap[idx]->idx = idx;
        _heap[min]->idx = min;
        idx = min;
    }
    return idx > pos;
}

void Timer::_shiftUp(int pos)
{
    for (;;) {
        int parent = (pos - 1) / 2; // parent node
        if (parent == pos || !_lessThan(pos, parent)) {
            break;
        }
        std::swap(_heap[parent], _heap[pos]);
        _heap[parent]->idx = parent;
        _heap[pos]->idx = pos;
        pos = parent;
    }
}

