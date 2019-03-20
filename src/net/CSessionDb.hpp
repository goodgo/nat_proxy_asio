/*
 * CSessionDb.hpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSESSIONDB_HPP_
#define SRC_NET_CSESSIONDB_HPP_

#include <string>
#include <deque>
#include <boost/unordered/unordered_map.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/atomic.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "CProtocol.hpp"

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

class CSessionDb
{
public:
	typedef boost::unordered_map<uint32_t, SSessionInfo> DbMap;
	typedef std::deque<OperationItem> OperationStack;
	typedef boost::shared_ptr<std::string> OutBuff;

	CSessionDb();
	~CSessionDb();
	void stop();
	void start();
	void worker();
	void add(const SSessionInfo& info);
	void del(uint32_t id);
	OutBuff output();

private:
	void operate();
	void serial();

public:
	boost::shared_ptr<boost::thread> _thread;
	DbMap _db;
	boost::mutex _op_mutex;
	boost::condition _op_cond;
	OperationStack _op_stack;

	boost::mutex _out_mutex;
	OutBuff _out_buff;
	enum { MAX_LENGTH = 2048 };
	char	_buff[MAX_LENGTH];
	int		_buff_len;
	bool	_started;
};

#endif /* SRC_NET_CSESSIONDB_HPP_ */
