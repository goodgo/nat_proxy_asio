/*
 * CSessionDb.cpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#include "CSessionDb.hpp"
#include <deque>
#include "util/CLogger.hpp"
#include "util/util.hpp"

using namespace redisclient;

CSessionDb::CSessionDb(asio::io_context& io_context, std::string addr, uint16_t port, std::string passwd)
: _redis(io_context)
, _redis_port(port)
, _redis_addr(addr)
, _redis_passwd(passwd)
, _thread()
, _out_buff(std::make_shared<std::string>(""))
, _buff_len(0)
, _started(false)
{

}

CSessionDb::~CSessionDb()
{
	stop();
}

void CSessionDb::start()
{
	_started = true;

	asio::ip::tcp::endpoint ep(
			asio::ip::make_address(_redis_addr), _redis_port);

	_redis.connect(ep, [&](boost::system::error_code ec){
		if (ec)
			LOG_ERROR << "redis connect error: " << ec.message();
		else
			_redis.command(kRedisAUTH, {_redis_passwd}, [](const RedisValue &result){
				LOG_INFO << "redis authenticated: " << result.toString();
			});
	});

	_thread = std::make_shared<std::thread>([&](){
		LOG_INFO << "session db thread start.";
		while(_started) {
			operate();
			serial();
		}
		LOG_INFO << "session db thread exit.";
	});
	_thread->detach();
}

void CSessionDb::stop()
{
	if (_started) {
		_started = false;
		_redis.disconnect();
		_op_cond.notify_all();
		LOG_INFO << "session db stopped!";
	}
}

void CSessionDb::operate()
{
	std::unique_lock<std::mutex> lk(_op_mutex);
	_op_cond.wait(lk, [this](){ return !_op_stack.empty(); });
	if (!_started)
		return;

	const auto& item = _op_stack.front();

	switch(item.op)	{
	case OPT::ADD: {
		const auto& info = item.info;
		_db.insert(std::make_pair(info.uiId, info));

		if (_redis.isConnected()) {
			std::deque<RedisBuffer> args;
			std::string key = kRedisKeySession+info.sGuid;
			args.push_back(key);

			args.push_back(kRedisFieldSessionId);
			std::string value_id = util::to_string(info.uiId);
			args.push_back(value_id);

			args.push_back(kRedisFieldSessionAddr);
			std::string value_addr = util::to_string(info.uiAddr);
			args.push_back(value_addr);

			args.push_back(kRedisFieldSessionGuid);
			args.push_back(info.sGuid);

			_redis.command(kRedisHMSET, args, [key](const RedisValue &result){
				LOG_INFO << "redis set session[" << key << "] completed: " << result.inspect();
			});
		}

		LOG_INFO << "session db add[" << info.uiId << "] " << info.sGuid << "size: " << _db.size();
		break;
	}
	case OPT::DEL: {
		auto it = _db.find(item.info.uiId);
		if (it == _db.end())
			break;

		auto info = it->second;

		_db.erase(info.uiId);
		if (_redis.isConnected()) {
			std::deque<RedisBuffer> args;
			std::string key = kRedisKeySession+info.sGuid;
			args.push_back(key);
			args.push_back(kRedisFieldSessionId);
			args.push_back(kRedisFieldSessionAddr);
			args.push_back(kRedisFieldSessionGuid);
			_redis.command(kRedisHDEL, args, [key](const RedisValue &result){
				LOG_INFO << "redis del session[" << key << "] completed: " << result.inspect();
			});
		}
		LOG_INFO << "session db del[" << info.uiId << "] " << info.sGuid << " size: " << _db.size();
		break;
	}
	default:
		LOGF_ERROR << "session db error operation:" << item.op;
		break;
	};

	_op_stack.pop_front();
}

void CSessionDb::serial()
{
	bzero(_buff, MAX_LENGTH);

	uint8_t cnt = 0;
	auto max_cnt = static_cast<uint16_t>(MAX_LENGTH / SSessionInfo::size());
	auto it = _db.begin();
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
		std::lock_guard<std::mutex> lk(_out_mutex);
		_out_buff.reset(new std::string(_buff, _buff_len));
	}
}

void CSessionDb::add(const SSessionInfo& info)
{
	OperationItem item;
	item.op = OPT::ADD;
	item.info = info;
	{
		std::lock_guard<std::mutex> lk(_out_mutex);
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
		std::lock_guard<std::mutex> lk(_out_mutex);
		_op_stack.push_back(item);
	}
	_op_cond.notify_all();
}

CSessionDb::OutBuff CSessionDb::output()
{
	OutBuff buff;
	{
		std::lock_guard<std::mutex> lk(_out_mutex);
		buff = _out_buff;
	}
	return buff;
}
