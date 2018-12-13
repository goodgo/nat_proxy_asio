/*
 * CQueue.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#include "CQueue.hpp"
#include "CLogger.hpp"
#include "util.hpp"

CQueue::CQueue()
: _thread()
, _serialLen(0)
, _started(true)
{

}

CQueue::~CQueue()
{
	stop();
}

void CQueue::start()
{
	_thread = boost::make_shared<boost::thread>(
			boost::bind(&CQueue::worker, this));
	_thread->detach();
}

void CQueue::stop()
{
	if (_started) {
		_started = false;
	}
}

void CQueue::worker()
{
	LOGF(TRACE) << "start queue worker...";
	while(_started) {
		{
			boost::mutex::scoped_lock lk(_queMutex);
			while (_que.empty()) {
				_queCond.wait(lk);
			}

			const QueueItem& item = _que.front();

			LOGF(DEBUG) << "[Queue] get item: " << item.info.uiId
					<< ", size: " << _que.size();

			switch(item.op)	{
			case EN_ADD: {
				_sessionInfoMap.insert(std::make_pair(item.info.uiId, item.info));
				break;
			}
			case EN_DEL: {
				_sessionInfoMap.erase(item.info.uiId);
				break;
			}
			};

			_que.pop_front();
		}

		{
			boost::mutex::scoped_lock lk(_infoMapMutex);
			bzero(_serial, 1024);

			_serial[0] = static_cast<uint8_t>(_sessionInfoMap.size());
			InfoMap::iterator it = _sessionInfoMap.begin();
			for (_serialLen = 1; it != _sessionInfoMap.end() && _serialLen < 1024; ++it) {
				uint32_t id = it->second.uiId;
				uint32_t addr = it->second.uiAddr;

				memcpy(_serial + _serialLen, &id, 4);
				_serialLen += 4;

				memcpy(_serial + _serialLen, &addr, 4);
				_serialLen += 4;
			}
			_serial[_serialLen] = '\0';
			LOG(TRACE) << "session serial: " << util::to_hex(_serial, _serialLen);
		}
	}
}

void CQueue::add(const SClientInfo& info)
{
	QueueItem item;
	item.op = EN_ADD;
	item.info = info;
	{
		boost::mutex::scoped_lock lk(_queMutex);
		_que.push_back(item);
	}
	_queCond.notify_all();
}

void CQueue::del(uint32_t id)
{
	QueueItem item;
	item.op = EN_ADD;
	item.info.uiId = id;
	{
		boost::mutex::scoped_lock lk(_queMutex);
		_que.push_back(item);
	}
	_queCond.notify_all();
}

void CQueue::get(std::string& str)
{
	boost::mutex::scoped_lock lk(_infoMapMutex);
	str.assign(_serial, _serialLen);
}
