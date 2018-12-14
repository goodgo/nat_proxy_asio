/*
 * CSession.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#include "CSession.hpp"
#include "CLogger.hpp"
#include "CServer.hpp"
#include "CProtocol.hpp"
#include "util.hpp"
#include <iostream>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/thread/thread.hpp>

CSession::CSession(CServer& server, asio::io_context& io_context)
: _server(server)
, _strand(io_context)
, _socket(io_context)
, _timer(io_context)
, _id(0)
, _privateAddr(0)
, _guid("")
, _logined(false)
, _started(true)
{
}

CSession::~CSession()
{
	LOGF(TRACE) << "session destroy [s:" << this << "] [id:" << _id << "]";
	stop();
}

void CSession::stop()
{
	if (_started)
	{
		LOGF(WARNING) << "session close. id: " << _id << ", guid: " << _guid;
		_started = false;
		boost::system::error_code ec;
		_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		_socket.close(ec);
		_timer.cancel();
		_server.closeSession(shared_from_this());
		LOG(DEBUG) << "remove all src channel session id: " << _id;
		_srcChannels.removeAll();
	}
}

void CSession::start()
{
	doRead();
}

void CSession::doRead()
{
	if (_started)
	{
		LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] start read...";
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
}

void CSession::doWrite(StringPtr msg)
{
	_strand.post(
			boost::bind(
					&CSession::writeImpl,
					shared_from_this(),
					msg)
	);
}

void CSession::onReadHead(const boost::system::error_code& ec, const size_t bytes)
{
	if (ec) {
		LOGF(ERR) << "[s:" << this << "] [id:" << _id << "] read head error: "
					<< ec.message() << ", bytes: " << bytes;
		stop();
		return;
	}

	if (!checkHead()) {
		LOGF(WARNING) << "[s:" << this << "] [id:" << _id << "] check head failed.";
		doRead();
		return;
	}

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
	if (ec) {
		LOGF(ERR) << "[s:" << this << "] [id:" << _id << "] read body error: " << ec.message();
		stop();
		return;
	}

	const char* p = asio::buffer_cast<const char*>(_readBuf.data());
	const size_t n = _readBuf.size();

	LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] read package: "
			<<  util::to_hex(p, sizeof(_header) + _header.usBodyLen);

	switch (_header.ucFunc) {
	case EN_FUNC::HEARTBEAT:
		break;
	case EN_FUNC::LOGIN: {
		boost::shared_ptr<CReqLoginPkg> pkg = boost::make_shared<CReqLoginPkg>();
		pkg->deserialize(p, n);
		onLogin(pkg);
	}break;
	case EN_FUNC::ACCELATE: {
		boost::shared_ptr<CReqAccelationPkg> pkg = boost::make_shared<CReqAccelationPkg>();
		pkg->deserialize(p, n);
		onAccelate(pkg);
	}break;
	case EN_FUNC::GET_CONSOLES: {
		boost::shared_ptr<CReqGetConsolesPkg> pkg = boost::make_shared<CReqGetConsolesPkg>();
		pkg->deserialize(p, n);
		onGetConsoles(pkg);
	}break;
	default:
		break;
	}

	_readBuf.consume(sizeof(_header) + _header.usBodyLen);
	doRead();
}

void CSession::writeImpl(StringPtr msg)
{
	_sendQue.push_back(msg);
	if (_sendQue.size() > 1)
	{
		LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] queue size: "
				    << _sendQue.size() << "return.";
		return;
	}
	write();
}

void CSession::write()
{
	StringPtr& msg = _sendQue[0];

	LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] write[" << msg->length() << "]: "  << util::to_hex(*msg);
	asio::async_write(
			_socket,
			asio::buffer(msg->c_str(), msg->length()),
			_strand.wrap(
					boost::bind(
							&CSession::onWriteComplete,
							shared_from_this(),
							asio::placeholders::error,
							asio::placeholders::bytes_transferred
					)
			)
	);
}

void CSession::onWriteComplete(const boost::system::error_code& ec, const size_t bytes)
{
	LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] write complete[" << bytes << "]:" << util::to_hex(*_sendQue[0]);
	_sendQue.pop_front();

	if (ec) {
		LOGF(ERR) << "[s:" << this << "] [id:" << _id << "] write error: " << ec.message();
		return;
	}

	if (!_sendQue.empty()) {
		LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] queue size: " << _sendQue.size() << ", write next.";
		write();
	}
	else
		LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] queue empty.";
}

bool CSession::checkHead()
{
	const char* pbuf = asio::buffer_cast<const char*>(_readBuf.data());
	memcpy(&_header, pbuf, sizeof(SHeaderPkg));

	if ((_header.ucHead1 == EN_HEAD::H1 &&
			_header.ucHead2 == EN_HEAD::H2) &&
		(EN_SVR_VERSION::ENCRYP == _header.ucSvrVersion ||
				EN_SVR_VERSION::NOENCRYP == _header.ucSvrVersion))
	{
		LOGF(TRACE) << "[s:" << this << "] [id:" << _id << "] read head:"
				<< util::to_hex(pbuf, sizeof(SHeaderPkg));
		return true;
	}

	LOGF(ERR) << "[s:" << this << "] [id:" << _id << "] read header error: " << (int)_header.ucHead1 << "|"
			  << (int)_header.ucHead2 << "|" << (int)_header.ucSvrVersion;

	bzero(&_header, sizeof(SHeaderPkg));
	_readBuf.consume(2);
	return false;
}

void CSession::onLogin(boost::shared_ptr<CReqLoginPkg>& req)
{
	CRespLogin resp;
	if (!_logined) {
		_guid = req->szGuid;
		_privateAddr = req->uiPrivateAddr;

		_logined = _server.onLogin(shared_from_this());
	}

	if (_logined) {
		resp.err = 0;
		resp.uiId = id();

		LOGF(INFO) << "[s:" << this << "] [id:" << _id << "] [guid:" << guid()
				<< "] login success.";
	}
	else {
		resp.err = 0xFF;

		LOGF(INFO) << "[s:" << this << "] [id:" << _id << "] [guid:" << guid()
				<< "] login failed.";
	}

	StringPtr msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onAccelate(boost::shared_ptr<CReqAccelationPkg>& req)
{
	CRespAccelate resp;
	LOGF(INFO) << "[s:" << this << "] [id:" << _id << "] accelate to id: "
			<< req->uiDstId;

	CSession::SelfType dst = _server.getSession(req->uiDstId);
	if (dst && dst->id() != _id) {
		CChannel::SelfType chann = _server.createChannel(shared_from_this(), dst);
		chann->start();

		asio::ip::udp::socket::endpoint_type src_ep = chann->srcEndpoint();
		asio::ip::udp::socket::endpoint_type dst_ep = chann->dstEndpoint();

		CRespAccess resp_dst;
		resp_dst.uiSrcId = _id;
		resp_dst.uiUdpAddr = dst_ep.address().to_v4().to_uint();
		resp_dst.usUdpPort = dst_ep.port();
		resp_dst.uiPrivateAddr = _privateAddr;

		SHeaderPkg head;
		bzero(&head, sizeof(SHeaderPkg));
		memcpy(&head, &req->header, sizeof(SHeaderPkg));
		head.ucFunc = EN_FUNC::REQ_ACCESS;

		StringPtr msg = resp_dst.serialize(head);
		dst->doWrite(msg);

		resp.err = 0;
		resp.uiUdpAddr = src_ep.address().to_v4().to_uint();
		resp.usUdpPort = src_ep.port();
	}
	else {
		resp.err = 0xFF;
		resp.uiUdpAddr = 0;
		resp.usUdpPort = 0;

		LOGF(ERR) << "[s:" << this << "] [id:" << _id << "] not found: " << req->uiDstId;
	}

	StringPtr msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onGetConsoles(boost::shared_ptr<CReqGetConsolesPkg>& req)
{
	CRespGetClients resp;
	_server.getAllClients(shared_from_this(), resp.info);
	StringPtr msg = resp.serialize(req->header);
	doWrite(msg);
}
