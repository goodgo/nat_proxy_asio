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

#include <boost/asio/signal_set.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>

#include "CIoContextPool.hpp"
#include "CSession.hpp"

namespace asio {
	using namespace boost::asio;
}

class CServer : private boost::noncopyable
{
public:
	//typedef boost::shared_ptr<CServer> SelfType;
	CServer(std::string address, uint16_t port, size_t pool_size);
	~CServer();
	void start();
	void stop();
	bool onLogin(CSession::SelfType sess);

private:
	void startAccept();
	void onAccept(const boost::system::error_code& ec);
	uint32_t getSessionId() { return _sessionId++; }

private:
	CIoContextPool _ioContextPool;
	asio::signal_set _signalSets;
	asio::ip::tcp::acceptor _acceptor;
	CSession::SelfType _sessionPtr;

	boost::mutex _mutex;
	typedef boost::unordered_map<std::string, uint32_t> GuidMap;
	GuidMap _guidMap;
	typedef boost::unordered_map<uint32_t, CSession::SelfType> SessionMap;
	SessionMap _sessionMap;
	uint32_t _sessionId;
	bool _started;
};


#endif /* SRC_NET_CSERVER_HPP_ */
