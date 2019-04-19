/*
 * CSessionDb.hpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSESSIONDB_HPP_
#define SRC_NET_CSESSIONDB_HPP_

#include <atomic>
#include <memory>
#include <string>
#include <deque>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/asio/io_context.hpp>

#include "CProtocol.hpp"
#include "redisclient/redisasyncclient.h"

namespace asio {
	using namespace boost::asio;
}

namespace OPT{
	enum {
		ADD = 0,
		DEL = 1,
	};
}

typedef struct
{
	uint8_t op;
	SSessionInfo info;
}OperationItem;

const std::string kRedisKeySession = "SESSION:";
const std::string kRedisFieldSessionId = "ID";
const std::string kRedisFieldSessionAddr = "ADDR";
const std::string kRedisFieldSessionGuid = "GUID";

const std::string kRedisAUTH = "AUTH";
const std::string kRedisSET = "SET";
const std::string kRedisGET = "GET";
const std::string kRedisLPUSH = "LPUSH";
const std::string kRedisRPUSH = "RPUSH";
const std::string kRedisHMSET = "HMSET";
const std::string kRedisHMGET = "HMGET";
const std::string kRedisHDEL = "HDEL";

class CSessionDb
{
public:
	typedef std::unordered_map<uint32_t, SSessionInfo> DbMap;
	typedef std::deque<OperationItem> OperationStack;
	typedef std::shared_ptr<std::string> OutBuff;

	CSessionDb(asio::io_context& io_context, std::string addr, uint16_t port, std::string passwd);
	~CSessionDb();
	void stop();
	void start();
	void add(const SSessionInfo& info);
	void del(uint32_t id);
	OutBuff output();

private:
	void operate();
	void serial();

public:
	redisclient::RedisAsyncClient _redis;
	uint16_t 	_redis_port;
	std::string _redis_addr;
	std::string _redis_passwd;

	std::shared_ptr<std::thread> _thread;
	DbMap _db;

	std::mutex _op_mutex;
	std::condition_variable _op_cond;
	OperationStack _op_stack;

	std::mutex _out_mutex;
	OutBuff _out_buff;
	enum { MAX_LENGTH = 2048 };
	char	_buff[MAX_LENGTH];
	int		_buff_len;
	bool	_started;
};

#endif /* SRC_NET_CSESSIONDB_HPP_ */
