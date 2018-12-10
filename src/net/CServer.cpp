/*
 * CServer.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CServer.hpp"
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio/io_context.hpp>
#include "CLogger.hpp"

CServer::CServer(std::string address, uint16_t port, size_t pool_size)
: _ioContextPool(pool_size)
, _signalSets(_ioContextPool.getIoContext())
, _acceptor(_ioContextPool.getIoContext())
, _sessionPtr()
, _sessionId(1000)
, _started(true)
{
	_signalSets.add(SIGHUP);

	_signalSets.async_wait(boost::bind(&CServer::stop, this));

	asio::ip::tcp::endpoint ep(asio::ip::address::from_string(address), port);
	_acceptor.open(ep.protocol());
	if (!_acceptor.is_open())
		throw std::runtime_error("open acceptor failed.");

	_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.bind(ep);
	_acceptor.listen();
}

CServer::~CServer()
{

}

void CServer::stop()
{
	LOGF(WARNING) << "server stop.";
	_started = false;
}

void CServer::start()
{
	LOGF(TRACE) << "start server: " << _acceptor.local_endpoint();
	startAccept();
	_ioContextPool.run();
}

void CServer::startAccept()
{
	_sessionPtr.reset(new CSession(*this, _ioContextPool.getIoContext()));
	_acceptor.async_accept(
			_sessionPtr->socket(),
			boost::bind(&CServer::onAccept,
					this,
					asio::placeholders::error));
}

void CServer::onAccept(const boost::system::error_code& ec)
{
	LOGF(TRACE) << "accept conn: [" << _sessionPtr->socket().remote_endpoint()
			    << "] ec: " << ec.message();
	if (!ec) {
		_sessionPtr->startRead();
	}
	startAccept();
}

bool CServer::onLogin(CSession::SelfType sess)
{
	boost::mutex::scoped_lock lk(_mutex);
	GuidMap::iterator it = _guidMap.find(sess->guid());
	if (it != _guidMap.end()) {
		return false;
	}

	sess->id(getSessionId());
	_guidMap.insert(GuidMap::value_type(sess->guid(), sess->id()));
	_sessionMap.insert(SessionMap::value_type(sess->id(), sess));

	return true;
}

bool CServer::getAllClients(CSession::SelfType sess, std::vector<SClientInfo>& clients)
{
	boost::mutex::scoped_lock lk(_mutex);
	SessionMap::iterator it = _sessionMap.begin();
	for (; it != _sessionMap.end(); ++it) {
		if (std::string::npos != it->second->guid().find_first_of("ep")) {
			if (it->second == sess)
				continue;

			LOG(TRACE) << "get client: " << it->first;
			SClientInfo info;
			info.uiId = it->first;
			clients.push_back(info);
		}
	}
	return true;
}
