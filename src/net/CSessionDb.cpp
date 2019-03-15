/*
 * CSessionDb.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#include "CSessionDb.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"

CSessionDb::CSessionDb()
: _thread()
, _out_buff(new std::string(""))
, _buff_len(0)
, _started(true)
{

}

CSessionDb::~CSessionDb()
{
	stop();
}

void CSessionDb::start()
{
	_thread = boost::make_shared<boost::thread>(
			boost::bind(&CSessionDb::worker, this));
	_thread->detach();
}

void CSessionDb::stop()
{
	if (_started) {
		_started = false;
		_op_cond.notify_all();
		LOGF(TRACE) << "session db stop.";
	}
}

void CSessionDb::worker()
{
	LOGF(TRACE) << "session db thread start.";
	while(_started) {
		operate();
		serial();
	}
	LOGF(TRACE) << "session db thread exit.";
}

void CSessionDb::operate()
{
	boost::mutex::scoped_lock lk(_op_mutex);
	while (_op_stack.empty()) {
		_op_cond.wait(lk);
		if (!_started)
			return;
	}

	const OperationItem& item = _op_stack.front();

	switch(item.op)	{
	case OPT::ADD: {
		_db.insert(std::make_pair(item.info.uiId, item.info));
		LOG(TRACE) << "session db add[" << item.info.uiId << "] size: " << _db.size();
		break;
	}
	case OPT::DEL: {
		_db.erase(item.info.uiId);
		LOG(TRACE) << "session db del[" << item.info.uiId << "] size: " << _db.size();
		break;
	}
	default:
		LOG(TRACE) << "session db error operation:" << item.op;
		break;
	};

	_op_stack.pop_front();
}

void CSessionDb::serial()
{
	bzero(_buff, MAX_LENGTH);

	uint8_t cnt = 0;
	uint16_t max_cnt = static_cast<uint16_t>(MAX_LENGTH / SSessionInfo::size());
	DbMap::iterator it = _db.begin();
	for (_buff_len = 1; it != _db.end() && _buff_len < MAX_LENGTH && cnt < max_cnt; ++it) {
		uint32_t id = it->second.uiId;
		uint32_t addr = it->second.uiAddr;

		memcpy(_buff + _buff_len, &id, 4);
		_buff_len += 4;

		memcpy(_buff + _buff_len, &addr, 4);
		_buff_len += 4;

		cnt++;
	}
	_buff[0] = cnt;
	_buff[_buff_len] = '\0';

	{
		boost::mutex::scoped_lock lk(_out_mutex);
		_out_buff.reset(new std::string(_buff, _buff_len));
	}
}

void CSessionDb::add(const SSessionInfo& info)
{
	OperationItem item;
	item.op = OPT::ADD;
	item.info = info;
	{
		boost::mutex::scoped_lock lk(_op_mutex);
		_op_stack.push_back(item);
	}
	_op_cond.notify_all();
}

void CSessionDb::del(uint32_t id)
{
	OperationItem item;
	item.op = OPT::DEL;
	item.info.uiId = id;
	{
		boost::mutex::scoped_lock lk(_op_mutex);
		_op_stack.push_back(item);
	}
	_op_cond.notify_all();
}

CSessionDb::OutBuff CSessionDb::output()
{
	OutBuff buff;
	{
		boost::mutex::scoped_lock lk(_out_mutex);
		buff = _out_buff;
	}
	return buff;
}
