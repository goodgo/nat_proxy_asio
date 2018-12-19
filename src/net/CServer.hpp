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
#include <CSafeMap.hpp>
#include <CSessionDb.hpp>

#include "CIoContextPool.hpp"
#include "CSession.hpp"
#include "CChannel.hpp"
#include "CSafeSet.hpp"
#include "CSafeMap.hpp"
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
	boost::shared_ptr<std::string> getAllSessions(CSession::SelfType sess);
	CSession::SelfType getSession(CSession::SelfType sess, uint32_t id);
	CChannel::SelfType createChannel(CSession::SelfType src, CSession::SelfType ds);

private:
	void startAccept();
	void onAccept(const boost::system::error_code& ec);
	uint32_t allocSessionId() { return _session_id.fetch_add(1); }
	uint32_t allocChannelId() { return _channel_id.fetch_add(1); }

private:
	CIoContextPool _io_context_pool;
	asio::signal_set _signal_sets;
	asio::ip::tcp::acceptor _acceptor;
	CSession::SelfType _session_ptr;

	CSafeSet<std::string> _guid_set;
	CSafeMap<uint32_t, CSession> _session_map;
	CSessionDb _session_db;

	boost::atomic<uint32_t> _session_id;
	boost::atomic<uint32_t> _channel_id;
	bool _started;
};


#endif /* SRC_NET_CSERVER_HPP_ */
