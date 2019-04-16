/*
 * CServer.h
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSERVER_HPP_
#define SRC_NET_CSERVER_HPP_

#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <boost/system/error_code.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "CIoContextPool.hpp"
#include "CSessionMgr.hpp"
#include "CSession.hpp"
#include "CSessionDb.hpp"
#include "CChannel.hpp"
#include "util/CSafeSet.hpp"
#include "util/util.hpp"

namespace asio {
	using namespace boost::asio;
}

class CServer : private std::enable_shared_from_this<CServer>
{
public:
	CServer(uint32_t pool_size);
	~CServer();
	bool init();
	bool start();
	void stop();
	void sessionClosed(const SessionPtr& ss);
	void signalHandle(const boost::system::error_code& ec, int sig);
	asio::io_context& getContext();

private:
	void acceptor(uint32_t id, asio::ip::tcp::endpoint ep, asio::yield_context yield);

private:
	CIoContextPool _io_context_pool;
	asio::signal_set _signal_sets;
	std::shared_ptr<CSessionMgr> _session_mgr;
	std::atomic<uint32_t> _conn_num;
	bool _started;
};

typedef std::shared_ptr<CServer> ServerPtr;
typedef std::weak_ptr<CServer> ServerWptr;

#endif /* SRC_NET_CSERVER_HPP_ */
