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
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/smart_ptr/scoped_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <vector>

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

class CServer : private boost::noncopyable
			  , public boost::enable_shared_from_this<CServer>
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
	boost::shared_ptr<CSessionMgr> _session_mgr;
	boost::atomic<uint32_t> _conn_num;
	bool _started;
};

typedef boost::shared_ptr<CServer> ServerPtr;
typedef boost::weak_ptr<CServer> ServerWptr;


#endif /* SRC_NET_CSERVER_HPP_ */
