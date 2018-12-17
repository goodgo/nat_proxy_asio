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
, _buff_len(0)
, _sessions(new std::string(""))
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
			boost::mutex::scoped_lock lk(_que_mutex);
			while (_que.empty()) {
				_que_cond.wait(lk);
			}

			const QueueItem& item = _que.front();

			LOGF(DEBUG) << "[Queue] get item: " << item.info.uiId << ", size: " << _que.size();

			switch(item.op)	{
			case EN_ADD: {
				_info_map.insert(std::make_pair(item.info.uiId, item.info));
				break;
			}
			case EN_DEL: {
				_info_map.erase(item.info.uiId);
				break;
			}
			};

			_que.pop_front();
		}

		{
			bzero(_buff, MAX_LENGTH);

			_buff[0] = static_cast<uint8_t>(_info_map.size());
			InfoMap::iterator it = _info_map.begin();
			for (_buff_len = 1; it != _info_map.end() && _buff_len < MAX_LENGTH; ++it) {
				uint32_t id = it->second.uiId;
				uint32_t addr = it->second.uiAddr;

				memcpy(_buff + _buff_len, &id, 4);
				_buff_len += 4;

				memcpy(_buff + _buff_len, &addr, 4);
				_buff_len += 4;
			}
			_buff[_buff_len] = '\0';
			LOG(TRACE) << "session serial: " << util::to_hex(_buff, _buff_len);

			boost::mutex::scoped_lock lk(_mutex);
			LOG(TRACE) << "new sessions copy use_count: " << _sessions.use_count();
			_sessions.reset(new std::string(_buff, _buff_len));
		}
	}
}

void CQueue::add(const SClientInfo& info)
{
	QueueItem item;
	item.op = EN_ADD;
	item.info = info;
	{
		boost::mutex::scoped_lock lk(_que_mutex);
		_que.push_back(item);
	}
	_que_cond.notify_all();
}

void CQueue::del(uint32_t id)
{
	QueueItem item;
	item.op = EN_DEL;
	item.info.uiId = id;
	{
		boost::mutex::scoped_lock lk(_que_mutex);
		_que.push_back(item);
	}
	_que_cond.notify_all();
}

boost::shared_ptr<std::string> CQueue::get()
{
	boost::mutex::scoped_lock lk(_mutex);
	boost::shared_ptr<std::string> s = _sessions;
	return s;
}
