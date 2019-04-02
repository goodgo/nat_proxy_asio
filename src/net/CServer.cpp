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
		_signal_sets.cancel(ignored_ec);
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
	_signal_sets.add(SIGTERM);
	_signal_sets.add(SIGINT);
	_signal_sets.add(SIGABRT);
	_signal_sets.add(SIGSEGV);
#ifdef SIGQUIT
	_signal_sets.add(SIGQUIT);
#endif

	_signal_sets.async_wait(boost::bind(&CServer::signalHandle, shared_from_this(), _1, _2));
	_session_mgr = boost::make_shared<CSessionMgr>(shared_from_this(),
												   gConfig->redisAddr(),
												   gConfig->redisPort());
	_started = true;
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
		stop();
		return false;
	}

	LOG(INFO) << "******* "
			<< gConfig->procName()
			<< " Version: " << SERVER_VERSION_DATE
			<< " <Build: " << BUILD_VERSION
#if !defined(NDEBUG)
			<< "(D)"
#endif
			<< "> I/O Workers: " << _io_context_pool.workerNum()
			<< " *******";

	LOG(INFO) << gConfig->print();

	_session_mgr->start();

	std::vector<std::string> ips = gConfig->srvAddrs();
	for (size_t i = 0; i < ips.size(); i++) {
		asio::ip::tcp::endpoint ep(asio::ip::address::from_string(ips[i]), gConfig->listenPort());
		asio::spawn(getContext(),
				boost::bind(&CServer::acceptor, shared_from_this(), i+1, ep, boost::placeholders::_1));
	}
	_io_context_pool.run();// block here
	return true;
}


void CServer::acceptor(uint32_t id, asio::ip::tcp::endpoint listen_ep, asio::yield_context yield)
{
	boost::system::error_code ec;

	asio::ip::tcp::acceptor acceptor(getContext());
	acceptor.open(listen_ep.protocol(), ec);
	if (ec) {
		LOGF(ERR) << "acceptor[" << id << "] open [" << listen_ep << "] failed: " << ec.message();
		return;
	}

	acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(listen_ep, ec);
	if (ec) {
		LOGF(ERR) << "acceptor[" << id << "] bind [" << listen_ep << "] failed: " << ec.message();
		return;
	}

	acceptor.listen();

	LOG(INFO) << "acceptor[" << id << "] listenning...[" << listen_ep << "]";
	while(_started) {
		SessionPtr ss = boost::make_shared<CSession>(_session_mgr,
							 boost::ref(_io_context_pool.getIoContext()),
							 gConfig->loginTimeout());

		acceptor.async_accept(ss->socket(), yield[ec]);
		if (ec) {
			LOGF(ERR) << "acceptor[" << id << "] listen error: " << ec.message();
			continue;
		}

		if (!ss->socket().is_open()) {
			LOGF(ERR) << "acceptor[" << id << "] socket has disconnected.";
			continue;
		}

		const asio::ip::tcp::endpoint& conn_ep = ss->socket().remote_endpoint(ec);
		if (ec) {
			LOGF(ERR) << "acceptor[" << id << "] socket get remote endpoint error: " << ec.message();
			continue;
		}

		if (gConfig->connLimit() > 0 && _conn_num > gConfig->connLimit()) {
			LOG(WARNING) << "acceptor[" << id << "] [CONN@" << conn_ep
					<< "] exceed max connections: [" << gConfig->connLimit()
					<< "] conn will be closed.";
			continue;
		}

		_conn_num.fetch_add(1);
		LOG(INFO) << "acceptor[" << id << "] <- [CONN@" << conn_ep << "] {"
				<< _conn_num << "/" << gConfig->connLimit() << "}";
		ss->start();
	}
	LOGF(ERR) << "acceptor[" << id << "] exit.";
}

void CServer::sessionClosed(const SessionPtr& ss)
{
	_conn_num.sub(1);
	LOG(TRACE) << "server close connection [ID@" << ss->id() << "] {"
			<< _conn_num << "/" << gConfig->connLimit() << "}";
}
