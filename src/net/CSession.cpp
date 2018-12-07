/*
 * CSession.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#include "CSession.hpp"
#include "CServer.hpp"
#include "CProtocol.hpp"
#include <iostream>
#include <boost/asio/read.hpp>
#include <boost/thread/thread.hpp>

CSession::CSession(CServer& server, asio::io_context& io_context)
: _server(server)
, _strand(io_context)
, _socket(io_context)
, _timer(io_context)
, _id(0)
, _privateAddr(0)
, _guid("")
, _started(true)
{
}

CSession::~CSession()
{

}

void CSession::stop()
{
	if (_started)
	{
		_started = false;
		boost::system::error_code ec;
		_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		_socket.close(ec);
	}
}

void CSession::startRead()
{
	asio::async_read(
			_socket,
			_readBuf,
			asio::transfer_exactly(sizeof(SHeaderPkg)),
			_strand.wrap(
					boost::bind(
						&CSession::onReadHead,
						shared_from_this(),
						asio::placeholders::error,
						asio::placeholders::bytes_transferred))
	);
}

void CSession::onReadHead(const boost::system::error_code& ec, const size_t bytes)
{
	std::cout << "ThreadId: " << boost::this_thread::get_id()
			  << " enter[" << __FUNCTION__ << "] ec: " << ec.message() << std::endl;

	if (ec) {
		std::cout << "ThreadId: " << boost::this_thread::get_id()
				  << "read head error: " << ec.message() << std::endl;
		startRead();
	}

	if (!checkHead(_header)) {
		std::cout << "ThreadId: " << boost::this_thread::get_id()
			  	  << "[" << __FUNCTION__ << "] check head failed." << std::endl;
		startRead();
	}

	std::cout << "ThreadId: " << boost::this_thread::get_id()
		  	  << "[" << __FUNCTION__ << "] read body len:"  << _header.usBodyLen << " bytes." << std::endl;
	asio::async_read(
		_socket,
		_readBuf,
		asio::transfer_exactly(_header.usBodyLen),
		_strand.wrap(
				boost::bind(
						&CSession::onReadBody,
						shared_from_this(),
						asio::placeholders::error,
						asio::placeholders::bytes_transferred))
	);
}

void CSession::onReadBody(const boost::system::error_code& ec, const size_t bytes)
{
	std::cout << "ThreadId: " << boost::this_thread::get_id()
			  << " enter[" << __FUNCTION__ << "] ec: " << ec.message() << std::endl;

	if (ec) {
		std::cout << "ThreadId: " << boost::this_thread::get_id()
		  	      << "[" << __FUNCTION__ << "] read body error: " << ec.message()  << std::endl;
		startRead();
	}

	const char* p = asio::buffer_cast<const char*>(_readBuf.data());
	const size_t n = _readBuf.size();

	std::cout << "ThreadId: " << boost::this_thread::get_id()
			  << " [" << __FUNCTION__ << "] body function: " << _header.ucFunc << std::endl;

	switch (_header.ucFunc) {
	case EN_FUNC::HEARTBEAT:
		break;
	case EN_FUNC::LOGIN: {
		boost::shared_ptr<CReqLoginPkg> pkg(new CReqLoginPkg);
		pkg->convFromStream(p, n);
		onLogin(pkg);
	}break;
	case EN_FUNC::ACCELATE: {
		boost::shared_ptr<CReqAccelationPkg> pkg(new CReqAccelationPkg);
		pkg->convFromStream(p, n);
		onAccelate(pkg);
	}break;
	case EN_FUNC::GET_CONSOLES: {
		boost::shared_ptr<CReqGetConsolesPkg> pkg(new CReqGetConsolesPkg);
		pkg->convFromStream(p, n);
		onGetConsoles(pkg);
	}break;
	default:
		break;
	}

	_readBuf.consume(sizeof(_header) + _header.usBodyLen);
	startRead();
}


bool CSession::checkHead(SHeaderPkg& header)
{
	const char* pbuf = asio::buffer_cast<const char*>(_readBuf.data());
	memcpy(&header, pbuf, sizeof(SHeaderPkg));

	if ((header.ucHead1 == EN_HEAD::H1
			&& header.ucHead2 == EN_HEAD::H2) &&
		(EN_SVR_VERSION::ENCRYP == header.ucSvrVersion ||
				EN_SVR_VERSION::NOENCRYP == header.ucSvrVersion))
	{
		return true;
	}

	std::cout << "session read header error: " << (int)header.ucHead1 << "|" << (int)header.ucHead2 << "|" << (int)header.ucSvrVersion << std::endl;
	bzero(&header, sizeof(SHeaderPkg));
	_readBuf.consume(2);
	return false;
}

void CSession::onLogin(boost::shared_ptr<CReqLoginPkg>& req)
{
	_guid = req->szGuid;
	_privateAddr = req->uiPrivateAddr;

	bool ret = _server.onLogin(shared_from_this());
	std::cout << "guid: " << guid() << " id: " << id() << (ret ? " login ok." : " login failed.") << std::endl;

	CRespLogin resp;

	if (ret) {
		resp.err = 0;
		resp.uiId = id();
	}
	else {
		resp.err = 0xFF;
	}

	req->header.usBodyLen = resp.size();
	startRead();
}

void CSession::onAccelate(boost::shared_ptr<CReqAccelationPkg>& pkg)
{

}

void CSession::onGetConsoles(boost::shared_ptr<CReqGetConsolesPkg>& pkg)
{

}
