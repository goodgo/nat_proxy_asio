/*
 * CChannel.cpp
 *
 *  Created on: Dec 12, 2018
 *      Author: root
 */

#include "CChannel.hpp"
#include <boost/bind.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/ratio.hpp>
#include "CSession.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"


CChannel::CEnd::CEnd(asio::io_context& io, uint32_t rbuff_size, uint32_t sbuff_size)
: _strand(io)
, _socket(io, asio::ip::udp::v4())
, _owner_id(0)
, _buf(rbuff_size)
, _opened(false)
{
	_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_socket.set_option(asio::ip::udp::socket::send_buffer_size(sbuff_size));
	_socket.set_option(asio::ip::udp::socket::receive_buffer_size(rbuff_size));
}

CChannel::CEnd::~CEnd()
{

}

bool CChannel::CEnd::init(SessionPtr ss)
{
	boost::system::error_code ec;
	_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
	if (ec) {
		LOG(ERR) << _owner_id << " bind udp socket error: " << ec.message();
		return false;
	}

	_owner_ss = ss;
	_owner_id = ss->id();
	_local_ep = _socket.local_endpoint();
	_remote_ep = asio::ip::udp::endpoint(ss->socket().remote_endpoint().address(), 0);

	return true;
}

void CChannel::CEnd::stop()
{
	LOGF(INFO) << "channel stop end.";
	boost::system::error_code ec;
	_socket.close(ec);
	_owner_ss.reset();
}

CChannel::CChannel(asio::io_context& io,
		uint32_t id,
		uint32_t rbuff_size,
		uint32_t sbuff_size,
		uint32_t display_timeout)
: _id(id)
, _src_end(io, rbuff_size, sbuff_size)
, _dst_end(io, rbuff_size, sbuff_size)
, _strand(io)
, _display_timer(io)
, _display_timeout(display_timeout)
, _up_bytes(0)
, _up_packs(0)
, _down_bytes(0)
, _down_packs(0)
, _start_tp(boost::chrono::steady_clock::now())
, _started(false)
{
}

CChannel::~CChannel()
{
	stop();
	LOGF(TRACE) << "channel[" << _id << "].";
}

bool CChannel::init(const SessionPtr& src_ss, const SessionPtr& dst_ss)
{
	if (!src_ss->addSrcChannel(shared_from_this())) {
		LOG(ERR) << "channel[" << _id << "] add to source session[" << src_ss->id() << "] failed!";
		return false;
	}

	if (!dst_ss->addDstChannel(shared_from_this())) {
		src_ss->closeSrcChannel(shared_from_this());
		LOG(ERR) << "channel[" << _id << "] add to destination session[" << dst_ss->id() << "] failed!";
		return false;
	}

	if (!_src_end.init(src_ss)) {
		return false;
	}

	if (!_dst_end.init(dst_ss))
		return false;

	return true;
}

void CChannel::toStop()
{
	_strand.get_io_context().post(
			_strand.wrap(
					boost::bind(&CChannel::stop, shared_from_this())
			)
	);
	LOGF(TRACE) << "channel[" << _id
			<< "]("	 << _src_end.sessionId()
			<< ") --> (" << _dst_end.sessionId()
			<< ") will be stopped.";
}

void CChannel::stop()
{
	if (_started) {
		_started = false;

		SessionPtr ss = _src_end.session().lock();
		if (ss) {
			ss->closeSrcChannel(shared_from_this());
		}

		ss = _dst_end.session().lock();
		if (ss) {
			ss->closeDstChannel(shared_from_this());
		}
		_src_end.stop();
		_dst_end.stop();

		boost::system::error_code ignored_ec;
		_display_timer.cancel(ignored_ec);

		boost::chrono::duration<double> sec =
				boost::chrono::steady_clock::now() - _start_tp;

		LOG(INFO) << "channel[" << _id << "] closed. takes time: " << std::setprecision(3)
				<< (static_cast<uint32_t>(sec.count()))%(24*3600)/3600.0 << " h"
				<< " | TX packets(" << _up_packs << "): " << util::formatBytes(_up_bytes)
				<< " | RX packets(" << _down_packs << "):" << util::formatBytes(_down_bytes);
	}
}

void CChannel::start()
{
	if (!_started) {
		_started = true;

		LOG(TRACE) << "channel[" << _id << "] "
				<< "("<< _src_end.sessionId()
				<< ")["    << _src_end.remote()
				<< " --> " << _src_end.localPort()
				<< " --> " << _dst_end.localPort()
				<< " --> " << _dst_end.remote()
				<< "]("    << _dst_end.sessionId() << ") opened.";

		asio::spawn(_src_end._strand, boost::bind(
				&CChannel::uploader, shared_from_this(), boost::placeholders::_1));
		asio::spawn(_dst_end._strand, boost::bind(
				&CChannel::downloader, shared_from_this(), boost::placeholders::_1));
		asio::spawn(_strand, boost::bind(
				&CChannel::displayer, shared_from_this(), boost::placeholders::_1));
	}
}


void CChannel::uploader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	LOG(TRACE) << "channel[" << _id << "] "
			<< "("		<< _src_end.sessionId()
			<< ")["		<< _src_end.remote()
			<< " --> "	<< _src_end.localPort()
			<< " --> "	<< _dst_end.localPort()
			<< " --> "	<< _dst_end.remote()
			<< "]("		<< _dst_end.sessionId() << ") src start auth...";

	while(_started && !_src_end.opened()) {
		bytes = _src_end._socket.async_receive_from(asio::buffer(_src_end._buf), _src_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "](" 	<< _dst_end.sessionId() << ") src receive auth error: " << ec.message();
			continue;
		}

		LOG(DEBUG) << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _src_end.remote()
				<< " --> "	<< _src_end.localPort()
				<< "]("		<< _dst_end.sessionId()
				<< ") src read auth[" << bytes << "B]: " << util::to_hex(_src_end._buf.data(), bytes);

		if (doAuth(_src_end._buf.data(), bytes) && _dst_end.opened())
			_src_end._opened = true;
		else
			_src_end.zeroBuf();

		LOG(INFO) << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _src_end.remote()
				<< " <-- "	<< _src_end.localPort()
				<< "]("		<< _dst_end.sessionId()
				<< ") src echo auth[" << bytes << "B]: " << util::to_hex(_src_end._buf.data(), bytes);

		bytes = _src_end._socket.async_send_to(asio::buffer(_src_end._buf, bytes), _src_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			_src_end._opened = false;

			LOG(ERR) << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " <-- "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId()
					<< ") src echo auth[" << bytes << "B] error: " << ec.message();
			continue;
		}
	}

	LOG(TRACE) << "channel[" << _id
			<< "] ("   << _src_end.sessionId()
			<< ")["    << _src_end.remote()
			<< " --> " << _src_end.localPort()
			<< " --> " << _dst_end.localPort()
			<< " --> " << _dst_end.remote()
			<< "]("    << _dst_end.sessionId() << ") start upload...";

	asio::ip::udp::endpoint ep;
	while (_started) {
		bytes = _src_end._socket.async_receive_from(asio::buffer(_src_end._buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] "
					<< "(" 		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId() << ") upload receive error: " << ec.message();
			continue;
		}

		if (ep != _src_end.remote()) {
			if (ep.address() != _src_end.remote().address())
				continue;

			LOG(WARNING) << "channel[" << _id << "] "
					<< "source remote endpoint change: [" << _src_end.remote() << "] ==> [" << ep << "]";

			_src_end._remote_ep.port(ep.port());
		}

		if (bytes <= 2)
			continue;

		bytes = _dst_end._socket.async_send(asio::buffer(_src_end._buf, bytes), yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _dst_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId() << ") upload send error: " << ec.message();
			continue;
		}

		_up_bytes.add(bytes);
		_up_packs.add(1);
	}
	LOGF(TRACE) << "channel[" << _id << "] uploader exit!";
}

void CChannel::downloader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	LOG(TRACE) << "channel[" << _id << "] "
			<< "(" << _src_end.sessionId()
			<< ")["    << _src_end.remote()
			<< " <-- " << _src_end.localPort()
			<< " <-- " << _dst_end.localPort()
			<< " <-- " << _dst_end.remote()
			<< "]("    << _dst_end.sessionId() << ") dst start auth...";

	while(_started && !_dst_end._opened) {
		bytes = _dst_end._socket.async_receive_from(asio::buffer(_dst_end._buf), _dst_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] ("
					<< _src_end.sessionId()
					<< ")["		<< _dst_end.localPort()
					<< " <-- "	<< _dst_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") dst receive auth error: " << ec.message();
			continue;
		}

		if (doAuth(_dst_end._buf.data(), bytes))
			_dst_end._opened = true;
		else
			_dst_end.zeroBuf();

		LOG(INFO) << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _dst_end.localPort()
				<< " <-- "	<< _dst_end.remote()
				<< "]("		<< _dst_end.sessionId()
				<< ") dst echo auth[" << bytes << "B]: " << util::to_hex(_dst_end._buf.data(), bytes);

		bytes = _dst_end._socket.async_send_to(asio::buffer(_dst_end._buf, bytes), _dst_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _dst_end.localPort()
				<< " <-- "	<< _dst_end.remote()
				<< "]("		<< _dst_end.sessionId()
				<< ") dst echo auth[" << bytes << "B] error: " << ec.message();
		}
	}

	LOG(TRACE) << "channel[" << _id << "] "
			<< "(" 		<< _src_end.sessionId()
			<< ")["		<< _src_end.remote()
			<< " <-- "	<< _src_end.localPort()
			<< " <-- "	<< _dst_end.localPort()
			<< " <-- "	<< _dst_end.remote()
			<< "]("		<< _dst_end.sessionId() << ") start download...";

	_dst_end._socket.connect(_dst_end.remote(), ec);
	if (ec)
		LOG(ERR) << "channel[" << _id << "] " << "downloader connect error: " << ec.message();

	while (_started) {
		bytes = _dst_end._socket.async_receive(asio::buffer(_dst_end._buf), yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")[" 	<< _dst_end.localPort()
					<< " <-- "	<< _dst_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") download receive error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _src_end._socket.async_send_to(asio::buffer(_dst_end._buf, bytes), _src_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _dst_end.localPort()
					<< " <-- "	<< _src_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") download send error: " << ec.message();
			continue;
		}

		_down_bytes.add(bytes);
		_down_packs.add(1);
	}
	LOGF(TRACE) << "channel[" << _id << "] downloader exit!";
}

void CChannel::displayer(asio::yield_context yield)
{
	using namespace util;

	boost::system::error_code ec;
	uint64_t up_bytes_prev = 0;
	uint64_t down_bytes_prev = 0;
	uint64_t up_packs_prev = 0;
	uint64_t down_packs_prev = 0;

	while(_started) {
		_display_timer.expires_from_now(boost::chrono::seconds(_display_timeout));
		_display_timer.async_wait(yield[ec]);
		if (ec) {
			LOG(ERR) << "channel[" << _id << "] displayer timer error: " << ec.message();
			continue;
		}

		if (_up_packs == up_packs_prev && _down_packs == down_packs_prev)
			continue;

		LOG(INFO) << "channel[" << _id << "] (" << _src_end.sessionId()
				<< ")["			<< _src_end.remote()
				<< " <--> " 	<< _dst_end.remote()
				<< "]("			<< _dst_end.sessionId() << ") "
				<< "Tx("		<< formatBytes((_up_bytes - up_bytes_prev) / _display_timeout)
				<< "ps/"		<< (_up_packs - up_packs_prev) / _display_timeout
				<< " pps) {"	<< formatBytes(_up_bytes) << ", "  << _up_packs
				<< " p} | Rx("	<< formatBytes((_down_bytes - down_bytes_prev) / _display_timeout)
				<< "ps/"		<< (_down_packs - down_packs_prev) / _display_timeout
				<< " pps) {"	<< formatBytes(_down_bytes) << ", " << _down_packs
				<< " p}"
				;

		up_bytes_prev = _up_bytes;
		down_bytes_prev = _down_bytes;
		up_packs_prev = _up_packs;
		down_packs_prev = _down_packs;
	}
	LOGF(TRACE) << "channel[" << _id << "] displayer exit!";
}

bool CChannel::doAuth(const char* buf, const size_t bytes)
{
	if (bytes > 0)
		return true;
	return false;
}
