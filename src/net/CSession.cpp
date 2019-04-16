/*
 * CSession.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#include "CSession.hpp"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <chrono.hpp>

#include "CSessionMgr.hpp"
#include "CProtocol.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"

CSession::CSession(std::shared_ptr<CSessionMgr> mgr, asio::io_context& io_context, uint32_t timeout)
: _mgr(mgr)
, _strand(io_context)
, _socket(io_context)
, _timer(io_context)
, _timeout(timeout)
, _id(0)
, _session_type(0)
, _private_addr(0)
, _guid("")
, _logined(false)
, _started(false)
{
}

CSession::~CSession()
{
	stop();
	LOGF(TRACE) << "session[" << _id << "].";
}

std::shared_ptr<CSession> CSession::newSession(std::shared_ptr<CSessionMgr>& mgr, asio::io_context& io_context, uint32_t timeout)
{
	return std::make_shared<CSession>(mgr, io_context, timeout);
}

void CSession::stop()
{
	boost::system::error_code ignored_ec;
	_timer.cancel(ignored_ec);
	_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
	_socket.close(ignored_ec);

	if (_started)
	{
		_started = false;

		if (_src_channels.size() > 0)
			_src_channels.stopAll();

		if (_dst_channels.size() > 0)
			_dst_channels.stopAll();

		_mgr->closeSessionWithLock(shared_from_this());
		LOGF(DEBUG) << "session[" << _id << "] stopped.";
	}
}

void CSession::start()
{
	if (!_started) {
		_started = true;

		_socket.set_option(asio::ip::tcp::no_delay(true));
		asio::spawn(_strand, std::bind(&CSession::reader, shared_from_this(), std::placeholders::_1));

		auto self = shared_from_this();
		_timer.expires_from_now(std::chrono::seconds(_timeout));
		_timer.async_wait([self](const boost::system::error_code& ec){
			if (!ec)
				self->stop();
		});
	}
}

void CSession::reader(asio::yield_context yield)
{
	boost::system::error_code ec;
	while (_started) {
		size_t bytes = asio::async_read(_socket, _rbuf, asio::transfer_exactly(sizeof(TagPktHdr)), yield[ec]);
		if (ec || 0 == bytes) {
			LOG(ERR) << "session[" << _id << "] read head error: " << ec.message();
			stop();
			break;
		}

		if (!checkHead()) {
			stop();
			break;
		}

		bytes = asio::async_read(_socket, _rbuf, asio::transfer_exactly(_hdr.usBodyLen), yield[ec]);
		if (ec || 0 == bytes) {
			LOGF(ERR) << "session[" << _id << "] read body error: " << ec.message();;
			stop();
			break;
		}

		auto pbuf = asio::buffer_cast<const char*>(_rbuf.data());
		const size_t nmax = _rbuf.size();

		LOG(TRACE) << "session[" << _id << "] read(#REQ_" << _mgr->getFuncName(_hdr.ucFunc) << "#): "
				<< util::to_hex(pbuf, sizeof(_hdr) + _hdr.usBodyLen);

		switch (_hdr.ucFunc) {
		case FUNC::REQ::HEARTBEAT:
			break;
		case FUNC::REQ::LOGIN: {
			std::shared_ptr<CReqLoginPkt> pkt = std::make_shared<CReqLoginPkt>();
			pkt->deserialize(pbuf, nmax);
			onReqLogin(pkt);
		}break;
		case FUNC::REQ::PROXY: {
			if (_session_type == SESSIONTYPE::CLIENT || _session_type == SESSIONTYPE::ANYONE) {
				std::shared_ptr<CReqProxyPkt> pkt = std::make_shared<CReqProxyPkt>();
				pkt->deserialize(pbuf, nmax);
				onReqProxy(pkt);
			}
		}break;
		case FUNC::REQ::GETPROXIES: {
			if (_session_type == SESSIONTYPE::CLIENT || _session_type == SESSIONTYPE::ANYONE) {
				std::shared_ptr<CReqGetProxiesPkt> pkt = std::make_shared<CReqGetProxiesPkt>();
				pkt->deserialize(pbuf, nmax);
				onReqGetProxies(pkt);
			}
		}break;
		default:
			break;
		}

		_rbuf.consume(sizeof(_hdr) + _hdr.usBodyLen);
	}

	LOG(INFO) << "session[" << _id << "] reader exit!";
}

void CSession::doWrite(const StringPtr& pkt)
{
	if (pkt->length() <= 0)
		return;

	auto self = shared_from_this();
	_strand.post([self, pkt](){
		self->_sque.push_back(pkt);
		if (self->_sque.size() > 1) {
			LOGF(TRACE) << "session[" << self->_id << "] queue size " << self->_sque.size() << " > 1 return.";
			return;
		}

		while (self->_sque.size() > 0) {
			const auto& pkt0 = self->_sque[0];
			asio::async_write(self->_socket, asio::buffer(pkt0->c_str(), pkt0->length()),
					self->_strand.wrap([self, &pkt0](const boost::system::error_code& ec, const size_t bytes){
						LOG(TRACE) << "session[" << self->_id << "] write(#RESP_" << self->_mgr->getFuncName((*pkt0)[4]) << "#): "
								<< util::to_hex(*pkt0);

						self->_sque.pop_front();
						if (ec) {
							LOGF(ERR) << "session[" << self->_id << "] write failed(" << bytes << "B): [" << util::to_hex(*pkt0)
									<< "] [error: " << ec.message() << "]";
							return;
						}
					})
			);
		}
	});
}

/*
void CSession::doRead()
{
	if (_started)
	{
		asio::async_read(
			_socket,
			_rbuf,
			asio::transfer_exactly(sizeof(TagPktHdr)),
			_strand.wrap(
					std::bind(
						&CSession::onReadHead,
						shared_from_this(),
						std::placeholders::_1,
						std::placeholders::_2))
		);
	}
}

void CSession::onReadHead(const boost::system::error_code& ec, const size_t bytes)
{
	if (ec || 0 == bytes) {
		LOG(ERR) << "session[" << _id << "] read head error: " << ec.message();
		stop();
		return;
	}

	if (!checkHead()) {
		stop();
		return;
	}

	asio::async_read(
		_socket,
		_rbuf,
		asio::transfer_exactly(_hdr.usBodyLen),
		_strand.wrap(
				std::bind(
						&CSession::onReadBody,
						shared_from_this(),
						std::placeholders::_1,
						std::placeholders::_2))
	);
}

void CSession::onReadBody(const boost::system::error_code& ec, const size_t bytes)
{
	if (ec || 0 == bytes) {
		LOGF(ERR) << "session[" << _id << "] read body error: " << ec.message();;
		stop();
		return;
	}

	const char* pbuf = asio::buffer_cast<const char*>(_rbuf.data());
	const size_t nmax = _rbuf.size();

	LOG(TRACE) << "session[" << _id << "] read(#REQ_" << _mgr->getFuncName(_hdr.ucFunc) << "#): "
			<< util::to_hex(pbuf, sizeof(_hdr) + _hdr.usBodyLen);

	switch (_hdr.ucFunc) {
	case FUNC::REQ::HEARTBEAT:
		break;
	case FUNC::REQ::LOGIN: {
		std::shared_ptr<CReqLoginPkt> pkt = std::make_shared<CReqLoginPkt>();
		pkt->deserialize(pbuf, nmax);
		onReqLogin(pkt);
	}break;
	case FUNC::REQ::PROXY: {
		if (_session_type == SESSIONTYPE::CLIENT || _session_type == SESSIONTYPE::ANYONE) {
			std::shared_ptr<CReqProxyPkt> pkt = std::make_shared<CReqProxyPkt>();
			pkt->deserialize(pbuf, nmax);
			onReqProxy(pkt);
		}
	}break;
	case FUNC::REQ::GETPROXIES: {
		if (_session_type == SESSIONTYPE::CLIENT || _session_type == SESSIONTYPE::ANYONE) {
			std::shared_ptr<CReqGetProxiesPkt> pkt = std::make_shared<CReqGetProxiesPkt>();
			pkt->deserialize(pbuf, nmax);
			onReqGetProxies(pkt);
		}
	}break;
	default:
		break;
	}

	_rbuf.consume(sizeof(_hdr) + _hdr.usBodyLen);
	doRead();
}
*/

/*
void CSession::doWrite(const StringPtr& msg)
{
	if (msg->length() <= 0)
		return;

	_strand.post(
			std::bind(
					&CSession::writeImpl,
					shared_from_this(),
					msg)
	);
}

void CSession::writeImpl(const StringPtr& msg)
{
	_sque.push_back(msg);
	if (_sque.size() > 1)
	{
		LOGF(TRACE) << "session[" << _id << "] queue size: " << _sque.size() << "return.";
		return;
	}
	write();
}

void CSession::write()
{
	const StringPtr& msg = _sque[0];
	asio::async_write(
			_socket,
			asio::buffer(msg->c_str(), msg->length()),
			_strand.wrap(
					std::bind(&CSession::onWriteComplete,
							shared_from_this(),
							std::placeholders::_1,
							std::placeholders::_2
	)));
}

void CSession::onWriteComplete(const boost::system::error_code& ec, const size_t bytes)
{
	StringPtr msg = _sque[0];
	LOG(TRACE) << "session[" << _id << "] write(#RESP_" << _mgr->getFuncName((*msg)[4]) << "#): "
			<< util::to_hex(*msg);

	_sque.pop_front();
	if (ec) {
		LOGF(ERR) << "session[" << _id << "] write failed(" << bytes << "B): [" << util::to_hex(*msg)
				<< "] [error: " << ec.message() << "]";
		return;
	}

	if (!_sque.empty())
		write();
}
*/

bool CSession::checkHead()
{
	const char* pbuf = asio::buffer_cast<const char*>(_rbuf.data());
	memcpy(&_hdr, pbuf, sizeof(TagPktHdr));

	if ((_hdr.ucHead1 == HEADER::H1 &&
			_hdr.ucHead2 == HEADER::H2) &&
		(SVRVERSION::ENCRYP == _hdr.ucSvrVersion ||
				SVRVERSION::NOENCRYP == _hdr.ucSvrVersion))
	{
		return true;
	}

	LOGF(ERR) << "session[" << _id << "] read header error: " << std::hex
			<< " 0x"  << (int)_hdr.ucHead1
			<< " | 0x" << (int)_hdr.ucHead2
			<< " | 0x" << (int)_hdr.ucSvrVersion;

	bzero(&_hdr, sizeof(TagPktHdr));
	_rbuf.consume(sizeof(TagPktHdr));
	return false;
}

void CSession::onReqLogin(const std::shared_ptr<CReqLoginPkt>& req)
{
	CRespLogin resp;
	if (!_logined) {
		_guid = req->szGuid;
		_private_addr = req->uiPrivateAddr;
		_session_type = req->uctype;

		_logined = _mgr->onSessionLogin(shared_from_this());

		if (_logined) {
			resp.error(ERRCODE::SUCCESS);
			resp.id(_id);
			LOG(INFO) << "session[" << _id << "] (" << getType() << ") <" << guid() << "> login success.";

			boost::system::error_code ignored_ec;
			_timer.cancel(ignored_ec);
		}
		else {
			resp.error(ERRCODE::LOGINED_FAILED);
			LOGF(INFO) << "session[" << _id << "] <" << guid() << "> login failed.";
		}
	}
	else {
		resp.error(ERRCODE::REPEAT_LOGINED_ERR);
		resp.id(_id);
		LOGF(INFO) << "session[" << _id << "] <" << guid() << "> login repeat.";
	}

	auto msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onReqProxy(const std::shared_ptr<CReqProxyPkt>& req)
{
	LOGF(INFO) << "session[" << _id << "] request proxy destination[" << req->uiDstId << "]";

	auto chann = _mgr->createChannel(shared_from_this(), req->uiDstId);
	if (!chann) {
		LOGF(ERR) << "session[" << _id << "] request proxy destination[" << req->uiDstId << "] no found.";
		onRespProxyErr(req, ERRCODE::CREATE_UDP_FAILED);
		return;
	}

	auto dst_ss = chann->getDstSession().lock();
	if (!dst_ss) {
		LOGF(ERR) << "session[" << req->uiDstId << "] invalid!";
		return;
	}
	if (!dst_ss->isRuning()) {
		LOGF(ERR) << "session[" << req->uiDstId << "] no running";
		return;
	}

	chann->start();
	dst_ss->onRespAccess(req, chann);
	onRespProxyOk(req, chann);
	return;
}

void CSession::onReqGetProxies(const std::shared_ptr<CReqGetProxiesPkt>& req)
{
	StringPtr msg;
	{
		CRespGetProxies resp;
		resp.sessions = _mgr->getAllSessions();
		msg = resp.serialize(req->header);
	}
	doWrite(msg);
	LOG(TRACE) << "session[" << _id << "] get sessions: " << util::to_hex(*msg);
}

std::string CSession::getType()
{
	switch(_session_type) {
	case SESSIONTYPE::CLIENT:
		return "CLIENT";
	case SESSIONTYPE::SERVER:
		return "SERVER";
	case SESSIONTYPE::ANYONE:
		return "ANYONE";
	default:
		return "NONE";
	}
}

bool CSession::addSrcChannel(const ChannelPtr& chann)
{
	bool ret = _src_channels.insert(chann->id(), chann);
	LOG(INFO) << "session[" << _id << "] has src channels: " << _src_channels.size()
			<< ", dst channels: " << _dst_channels.size() << (!ret ? "failed!" : "");
	return ret;
}

bool CSession::addDstChannel(const ChannelPtr& chann)
{
	bool ret =  _dst_channels.insert(chann->id(), chann);
	LOG(INFO) << "session[" << _id << "] has dst channels: " << _dst_channels.size()
			<< ", src channels: " << _src_channels.size() << (!ret ? "failed!" : "");
	return ret;
}

void CSession::closeSrcChannel(const ChannelPtr& chann)
{
	LOGF(TRACE) << "session[" << _id << "] close channel: " << chann->id();
	_src_channels.remove(chann->id());
	onRespStopProxy(chann->id(), chann->srcEndpoint());
}

void CSession::closeDstChannel(const ChannelPtr& chann)
{
	LOGF(TRACE) << "session[" << _id << "] close channel: " << chann->id();
	_dst_channels.remove(chann->id());
	onRespStopProxy(chann->id(), chann->dstEndpoint());
}

void CSession::onRespAccess(const std::shared_ptr<CReqProxyPkt>& req, const ChannelPtr& chann)
{
	TagPktHdr hdr;
	bzero(&hdr, sizeof(TagPktHdr));
	memcpy(&hdr, &req->header, sizeof(TagPktHdr));
	hdr.ucFunc = FUNC::RESP::ACCESS;

	CRespAccess resp_dst;
	resp_dst.srcId(_id);
	resp_dst.udpId(chann->id());
	resp_dst.udpAddr(chann->dstEndpoint().address().to_v4().to_uint());
	resp_dst.udpPort(asio::detail::socket_ops::host_to_network_short(chann->dstEndpoint().port()));
	resp_dst.privateAddr(_private_addr);

	auto msg = resp_dst.serialize(hdr);
	doWrite(msg);
}

void CSession::onRespProxyOk(const std::shared_ptr<CReqProxyPkt>& req, const ChannelPtr& chann)
{
	CRespProxy resp;
	resp.error(ERRCODE::SUCCESS);
	resp.udpId(chann->id());
	resp.udpAddr(chann->srcEndpoint().address().to_v4().to_uint());
	resp.udpPort(asio::detail::socket_ops::host_to_network_short(chann->srcEndpoint().port()));

	auto msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onRespProxyErr(const std::shared_ptr<CReqProxyPkt>& req, uint8_t errcode)
{
	CRespProxy resp;
	resp.error(errcode);
	resp.udpId(0);
	resp.udpAddr(0);
	resp.udpPort(0);

	auto msg = resp.serialize(req->header);
	doWrite(msg);
}

void CSession::onRespStopProxy(uint32_t id, const asio::ip::udp::endpoint& ep)
{
	CRespStopProxy resp;
	resp.udpId(id);
	resp.udpAddr(ep.address().to_v4().to_uint());
	resp.udpPort(asio::detail::socket_ops::host_to_network_short(ep.port()));

	TagPktHdr hdr;
	hdr.ucHead1 = HEADER::H1;
	hdr.ucHead2 = HEADER::H2;
	hdr.ucPrtVersion = PROTOVERSION::V1;
	hdr.ucSvrVersion = SVRVERSION::NOENCRYP;
	hdr.ucFunc = FUNC::RESP::STOPPROXY;
	hdr.ucKeyIndex = 0x00;

	auto msg = resp.serialize(hdr);
	doWrite(msg);
}
