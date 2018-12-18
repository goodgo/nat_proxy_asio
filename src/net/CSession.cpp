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
, _id(~0)
, _private_addr(0)
, _guid("")
, _logined(false)
, _started(true)
{
}

CSession::~CSession()
{
	LOGF(TRACE) << "session[" << _id << "] destroy.";
	stop();
}

void CSession::stop()
{
	if (_started)
	{
		LOGF(WARNING) << "session[" << _id << "] stop.";
		_started = false;
		boost::system::error_code ec;
		_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
		_socket.close(ec);
		_timer.cancel(ec);
		_server.closeSession(shared_from_this());
		LOGF(DEBUG) << "session[" << _id << "] remove all src channel.";
		_src_channels.removeAll();
	}
}

void CSession::start()
{
	LOGF(DEBUG) << "session[" << _id << "] start.";
	doRead();
}

void CSession::doRead()
{
	if (_started)
	{
		LOGF(TRACE) << "session[" << _id << "] reading.";
		asio::async_read(
			_socket,
			_read_buf,
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
	if (msg->length() <= 0)
		return;

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
		LOG(ERR) << "session[" << _id << "] read head error: " << ec.message();;
		stop();
		return;
	}

	if (!checkHead()) {
		LOG(WARNING) << "session[" << _id << "] check head failed: "
				<< util::to_hex((char*)&_header, sizeof(_header));
		doRead();
		return;
	}

	asio::async_read(
		_socket,
		_read_buf,
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
		LOG(ERR) << "session[" << _id << "] read body error: " << ec.message();;
		stop();
		return;
	}

	const char* p = asio::buffer_cast<const char*>(_read_buf.data());
	const size_t n = _read_buf.size();

	LOG(ERR) << "session[" << _id << "] read package: "
			<< util::to_hex(p, sizeof(_header) + _header.usBodyLen);

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
		onGetSessions(pkg);
	}break;
	default:
		break;
	}

	_read_buf.consume(sizeof(_header) + _header.usBodyLen);
	doRead();
}

void CSession::writeImpl(StringPtr msg)
{
	_send_que.push_back(msg);
	if (_send_que.size() > 1)
	{
		LOGF(TRACE) << "session[" << _id << "] queue size: " << _send_que.size() << "return.";
		return;
	}
	write();
}

void CSession::write()
{
	StringPtr& msg = _send_que[0];

	LOGF(TRACE) << "session[" << _id << "] write(" << msg->length() << "): "  << util::to_hex(*msg);
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
	LOGF(TRACE) << "session[" << _id << "] write complete(" << bytes << "):" << util::to_hex(*_send_que[0]);
	_send_que.pop_front();

	if (ec) {
		LOGF(ERR) << "session[" << _id << "] write error: " << ec.message();
		return;
	}

	if (!_send_que.empty()) {
		LOGF(TRACE) << "session[" << _id << "] write queue size: " << _send_que.size() << ", write next.";
		write();
	}
	else
		LOGF(TRACE) << "session[" << _id << "] write queue empty.";
}

bool CSession::checkHead()
{
	const char* pbuf = asio::buffer_cast<const char*>(_read_buf.data());
	memcpy(&_header, pbuf, sizeof(SHeaderPkg));

	if ((_header.ucHead1 == EN_HEAD::H1 &&
			_header.ucHead2 == EN_HEAD::H2) &&
		(EN_SVR_VERSION::ENCRYP == _header.ucSvrVersion ||
				EN_SVR_VERSION::NOENCRYP == _header.ucSvrVersion))
	{
		return true;
	}

	LOGF(ERR) << "session[" << _id << "] read header error: "
			<< (int)_header.ucHead1 << "|"
			<< (int)_header.ucHead2 << "|"
			<< (int)_header.ucSvrVersion;

	bzero(&_header, sizeof(SHeaderPkg));
	_read_buf.consume(2);
	return false;
}

void CSession::onLogin(boost::shared_ptr<CReqLoginPkg>& req)
{
	CRespLogin resp;
	if (!_logined) {
		_guid = req->szGuid;
		_private_addr = req->uiPrivateAddr;

		_logined = _server.onLogin(shared_from_this());
	}

	if (_logined) {
		resp.ucErr = 0;
		resp.uiId = id();

		LOGF(INFO) << "session[" << _id << "] <" << guid() << "> login success.";
	}
	else {
		resp.ucErr = 0xFF;
		LOGF(INFO) << "session[" << _id << "] <" << guid() << "> login failed.";
	}

	StringPtr msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onAccelate(boost::shared_ptr<CReqAccelationPkg>& req)
{
	CRespAccelate resp;
	LOGF(INFO) << "session[" << _id << "] request accelate destination[" << req->uiDstId << "]";

	CSession::SelfType dst = _server.getSession(shared_from_this(), req->uiDstId);
	if (dst.get() != shared_from_this().get()) {
		CChannel::SelfType chann = _server.createChannel(shared_from_this(), dst);
		chann->start();

		asio::ip::udp::socket::endpoint_type src_ep = chann->srcEndpoint();
		asio::ip::udp::socket::endpoint_type dst_ep = chann->dstEndpoint();

		CRespAccess resp_dst;
		resp_dst.uiSrcId = _id;
		resp_dst.uiUdpAddr = dst_ep.address().to_v4().to_uint();
		resp_dst.usUdpPort = asio::detail::socket_ops::host_to_network_short(dst_ep.port());
		resp_dst.uiPrivateAddr = _private_addr;

		SHeaderPkg head;
		bzero(&head, sizeof(SHeaderPkg));
		memcpy(&head, &req->header, sizeof(SHeaderPkg));
		head.ucFunc = EN_FUNC::REQ_ACCESS;

		StringPtr msg = resp_dst.serialize(head);
		dst->doWrite(msg);

		resp.ucErr = 0;
		resp.uiUdpAddr = src_ep.address().to_v4().to_uint();
		resp.usUdpPort = asio::detail::socket_ops::host_to_network_short(src_ep.port());
	}
	else {
		resp.ucErr = 0xFF;
		resp.uiUdpAddr = 0;
		resp.usUdpPort = 0;

		LOGF(ERR) << "session[" << _id << "] request accelate destination[" << req->uiDstId << "] no found.";
	}

	StringPtr msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onGetSessions(boost::shared_ptr<CReqGetConsolesPkg>& req)
{
	StringPtr msg;
	{
		CRespGetSessions resp;
		resp.sessions = _server.getAllSessions(shared_from_this());
		msg = resp.serialize(req->header);
	}
	doWrite(msg);
	LOG(TRACE) << "session[" << _id << "] get sessions: " << util::to_hex(*msg);
}
