/*
 * CServer.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CServer.hpp"
#include <boost/bind.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio/io_context.hpp>
#include "CLogger.hpp"

CServer::CServer(std::string address, uint16_t port, size_t pool_size)
: _ioContextPool(pool_size)
, _signalSets(_ioContextPool.getIoContext())
, _acceptor(_ioContextPool.getIoContext())
, _sessionPtr()
, _sessionId(1000)
, _channelId(1)
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
	LOGF(TRACE);
}

void CServer::stop()
{
	LOGF(WARNING) << "server stop.";
	_started = false;
	_queue.stop();
}

void CServer::start()
{
	LOGF(TRACE) << "start server: " << _acceptor.local_endpoint();
	_queue.start();
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
		_sessionPtr->start();
	}
	startAccept();
}

void CServer::closeSession(CSession::SelfType sess)
{
	_guidSet.remove(sess->guid());
	_sessionMap.remove(sess->id());

	LOGF(TRACE) << "close session[" << sess << "]guid: " << sess->guid() << ", id: " << sess->id();
}

bool CServer::onLogin(CSession::SelfType sess)
{
	LOGF(TRACE) << "insert guid: " << sess->guid();
	if (!_guidSet.insert(sess->guid()))
	{
		LOGF(DEBUG) << "[s:" << sess << "] [guid:" << sess->guid() << "] login failed.";
		return false;
	}

	sess->id(allocSessionId());
	LOGF(TRACE) << "insert session: " << sess;
	if (!_sessionMap.insert(sess->id(), sess))
	{
		LOGF(DEBUG) << "[s:" << sess << "] [guid:" << sess->guid() << "] login failed.";
		return false;
	}

	SClientInfo info(sess->id(), sess->remoteAddr());
	_queue.add(info);

	return true;
}

CSession::SelfType CServer::getSession(uint32_t id)
{
	boost::weak_ptr<CSession> wptr = _sessionMap.get(id);
	if (wptr.expired()) {
		LOGF(DEBUG) << "get session id: " << id << " expired.";
		return CSession::SelfType(wptr);
	}
	CSession::SelfType ss = wptr.lock();
	return ss;
}

CChannel::SelfType CServer::createChannel(CSession::SelfType src, CSession::SelfType dst)
{
	asio::ip::address addr = _acceptor.local_endpoint().address();
	asio::ip::udp::endpoint src_ep(addr, 0);
	asio::ip::udp::endpoint dst_ep(addr, 0);

	CChannel::SelfType chann(new CChannel(
			_ioContextPool.getIoContext(),
			allocChannelId(),
			src, dst,
			src_ep, dst_ep));

	src->addSrcChannel(chann);
	dst->addDstChannel(chann);

	return chann;
}

bool CServer::getAllClients(CSession::SelfType sess, std::string& str)
{
	_queue.get(str);
	return true;
}
