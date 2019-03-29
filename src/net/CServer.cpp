/*
 * CServer.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CServer.hpp"
#include <boost/bind.hpp>
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio/io_context.hpp>
#include "util/version.h"
#include "util/CLogger.hpp"
#include "util/CConfig.hpp"

CServer::CServer(uint32_t pool_size)
: _io_context_pool(pool_size)
, _signal_sets(_io_context_pool.getIoContext())
, _acceptor(_io_context_pool.getIoContext())
, _session_ptr()
, _conn_num(0)
, _started(false)
{

}

CServer::~CServer()
{
	LOGF(TRACE);
	finitLog();
}

void CServer::signalHandle(const boost::system::error_code& ec, int sig)
{
	LOGF(ERR) << "server catch signal: " << sig << ", error: " << ec.message();
	stop();
}

void CServer::stop()
{
	if (_started) {
		_started = false;

		boost::system::error_code ignored_ec;
		_acceptor.cancel(ignored_ec);
		_signal_sets.cancel(ignored_ec);
		_session_ptr.reset();
		_session_mgr->stop();
		_io_context_pool.stop();

		LOG(WARNING) << "****************** Server Stopped. Version: "
				<< SERVER_VERSION_DATE << " <Build: " << BUILD_VERSION
#if !defined(NDEBUG)
				<< "(D)"
#endif
				<< "> ******************";

		exit(0);
	}
}

bool CServer::init()
{
	boost::system::error_code ec;

	_signal_sets.add(SIGTERM);
	_signal_sets.add(SIGINT);
	_signal_sets.add(SIGABRT);
	_signal_sets.add(SIGSEGV);
#ifdef SIGQUIT
	_signal_sets.add(SIGQUIT);
#endif

	_signal_sets.async_wait(boost::bind(&CServer::signalHandle, this, _1, _2));

	asio::ip::tcp::endpoint ep(
			asio::ip::address::from_string(gConfig->srvAddr()),
			gConfig->listenPort());
	_acceptor.open(ep.protocol(), ec);
	if (ec) {
		LOG(ERR) << "server open accept failed: " << ec.message();
		return false;
	}

	_acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	_acceptor.bind(ep, ec);
	if (ec) {
		LOG(ERR) << "server bind[" << ep << "] failed: " << ec.message();
		return false;
	}

	_acceptor.listen();
	_started = true;
	_session_mgr.reset(new CSessionMgr(boost::ref(*this)));
	return true;
}

asio::io_context& CServer::getContext()
{
	return _io_context_pool.getIoContext();
}

bool CServer::start()
{
	if (!init()) {
		LOG(ERR) << "server init failed!";
		return false;
	}

	LOG(INFO) << "******* "
			<< gConfig->procName()
			<< " Version: " << SERVER_VERSION_DATE
			<< " <Build: " << BUILD_VERSION
#if !defined(NDEBUG)
			<< "(D)"
#endif
			<< "> Cores: " << _io_context_pool.workerNum()
			<< ", service: [" << _acceptor.local_endpoint() << "]"
			<< " *******";

	LOG(INFO) << gConfig->print();

	_session_mgr->start();
	startAccept();
	_io_context_pool.run();// block here
	return true;
}

void CServer::startAccept()
{
	_session_ptr.reset(new CSession(_session_mgr,
			boost::ref(_io_context_pool.getIoContext()),
			gConfig->loginTimeout()));

	if (!_acceptor.is_open()) {
		LOGF(FATAL) << "acceptor has been closed!";
		return;
	}

	_acceptor.async_accept(
			_session_ptr->socket(),
			boost::bind(&CServer::onAccept,
					this,
					asio::placeholders::error));
}

void CServer::onAccept(const boost::system::error_code& ec)
{
	if (ec) {
		LOGF(ERR) << "accept error: " << ec.message();
		return;
	}

	if (!_session_ptr) {
		LOGF(ERR) << "accept error: session point exception!";
		return;
	}

	boost::system::error_code ec2;
	asio::ip::tcp::endpoint ep = _session_ptr->socket().remote_endpoint(ec2);
	if (ec2) {
		LOGF(ERR) << "accept error: " << ec2;
		return;
	}

	_conn_num.fetch_add(1);
	LOGF(INFO) << "accept [CONN@" << ep << "] {N:" << _conn_num << "}";
	_session_ptr->start();
	startAccept();
}

void CServer::sessionClosed(const SessionPtr& ss)
{
	_conn_num.sub(1);
	LOGF(TRACE) << "server close connection [ID@" << ss->id() << "] {N:" << _conn_num << "}";
}
