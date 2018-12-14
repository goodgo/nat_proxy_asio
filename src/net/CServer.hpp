/*
 * CServer.h
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSERVER_HPP_
#define SRC_NET_CSERVER_HPP_

#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>
#include <boost/system/error_code.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>

#include "CIoContextPool.hpp"
#include "CSession.hpp"
#include "CChannel.hpp"
#include "CQueue.hpp"
#include "util.hpp"

namespace asio {
	using namespace boost::asio;
}

class CServer : private boost::noncopyable
{
public:
	CServer(std::string address, uint16_t port, size_t pool_size);
	~CServer();
	void start();
	void stop();
	void closeSession(CSession::SelfType sess);
	bool onLogin(CSession::SelfType sess);
	bool getAllClients(CSession::SelfType sess, std::string& str);
	CSession::SelfType getSession(uint32_t id);
	CChannel::SelfType createChannel(CSession::SelfType src, CSession::SelfType ds);

private:
	void startAccept();
	void onAccept(const boost::system::error_code& ec);
	uint32_t allocSessionId() { return _sessionId.fetch_add(1); }
	uint32_t allocChannelId() { return _channelId.fetch_add(1); }

private:
	CIoContextPool _ioContextPool;
	asio::signal_set _signalSets;
	asio::ip::tcp::acceptor _acceptor;
	CSession::SelfType _sessionPtr;

	util::DataSet<std::string> _guidSet;
	util::DataMap<uint32_t, CSession> _sessionMap;
	CQueue _queue;

	boost::atomic<uint32_t> _sessionId;
	boost::atomic<uint32_t> _channelId;
	bool _started;
};


#endif /* SRC_NET_CSERVER_HPP_ */
