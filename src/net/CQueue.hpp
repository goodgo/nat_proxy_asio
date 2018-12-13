/*
 * CQueue.hpp
 *
 *  Created on: Dec 11, 2018
 *      Author: root
 */

#ifndef SRC_NET_CQUEUE_HPP_
#define SRC_NET_CQUEUE_HPP_

#include <map>
#include <string>
#include <sstream>
#include <deque>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/atomic.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include "CProtocol.hpp"

enum
{
	EN_ADD = 0,
	EN_DEL = 1,
};

typedef struct
{
	uint8_t op;
	SClientInfo info;
}QueueItem;

class CQueue
{
public:
	typedef std::map<uint32_t, SClientInfo> InfoMap;

	CQueue();
	~CQueue();
	void stop();
	void start();
	void worker();
	void add(const SClientInfo& info);
	void del(uint32_t id);
	void get(std::string& str);

public:
	boost::shared_ptr<boost::thread> _thread;
	boost::mutex _infoMapMutex;
	boost::mutex _queMutex;
	boost::condition _queCond;
	std::deque<QueueItem> _que;
	InfoMap _sessionInfoMap;
	int		_serialLen;
	char	_serial[1024];
	bool	_started;
};


#endif /* SRC_NET_CQUEUE_HPP_ */
