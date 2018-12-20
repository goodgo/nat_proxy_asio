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
#include "version.h"

CServer::CServer(std::string address, uint16_t port, size_t pool_size)
: _io_context_pool(pool_size)
, _signal_sets(_io_context_pool.getIoContext())
, _acceptor(_io_context_pool.getIoContext())
, _session_ptr()
, _session_id(1000)
, _channel_id(1)
, _started(true)
{
	_signal_sets.add(SIGTERM);

	_signal_sets.async_wait(boost::bind(&CServer::stop, this));

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
	FinitLog();
}

void CServer::stop()
{
	LOGF(WARNING) << "server stop.";
	_started = false;
	_session_db.stop();
}

void CServer::start()
{
	LOG(TRACE) << "start server: " << _acceptor.local_endpoint()
			<< ", version: " << SERVER_VERSION_DATE << " <buildin: " << BUILD_VERSION << ">";
	_session_db.start();
	startAccept();
	_io_context_pool.run();
}

void CServer::startAccept()
{
	_session_ptr.reset(new CSession(*this, _io_context_pool.getIoContext()));
	_acceptor.async_accept(
			_session_ptr->socket(),
			boost::bind(&CServer::onAccept,
					this,
					asio::placeholders::error));
}

void CServer::onAccept(const boost::system::error_code& ec)
{
	LOGF(TRACE) << "accept conn: [" << _session_ptr->socket().remote_endpoint()
			<< "] ec: " << ec.message();
	if (!ec) {
		LOGF(TRACE);
		_session_ptr->start();
	}
	startAccept();
}

void CServer::closeSession(CSession::SelfType sess)
{
	_guid_set.remove(sess->guid());
	_session_map.remove(sess->id());
	_session_db.del(sess->id());

	LOGF(TRACE) << "server close session[" << sess->id() << "] [" << sess->guid() << "]";
}

bool CServer::onLogin(CSession::SelfType sess)
{
	if (!_guid_set.insert(sess->guid())) {
		LOGF(ERR) << "[guid: " << sess->guid() << "] insert failed.";
		return false;
	}

	sess->id(allocSessionId());
	if (!_session_map.insert(sess->id(), sess)) {
		LOGF(ERR) << "session id[: " << sess->id() << "] insert failed.";
		return false;
	}

	SSessionInfo info(sess->id(), sess->remoteAddr());
	_session_db.add(info);

	return true;
}

CSession::SelfType CServer::getSession(CSession::SelfType sess, uint32_t id)
{
	boost::weak_ptr<CSession> wptr = _session_map.get(id);
	if (wptr.expired()) {
		LOGF(DEBUG) << "session[" << sess->id() << "] get dest[" << id << " ] failed.";
		return sess;
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
			_io_context_pool.getIoContext(),
			allocChannelId(),
			src, dst,
			src_ep, dst_ep));

	src->addSrcChannel(chann);
	dst->addDstChannel(chann);

	return chann;
}

boost::shared_ptr<std::string> CServer::getAllSessions(CSession::SelfType sess)
{
	return _session_db.output();
}
