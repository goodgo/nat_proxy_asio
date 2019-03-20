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

CChannel::CChannel(asio::io_context& io,
		uint32_t id,
		uint32_t rbuff_size,
		uint32_t sbuff_size,
		uint32_t display_timeout)
: _src_strand(io)
, _dst_strand(io)
, _display_timer(io)
, _src_socket(io, asio::ip::udp::v4())
, _dst_socket(io, asio::ip::udp::v4())
, _src_buf(rbuff_size)
, _dst_buf(rbuff_size)
, _id(id)
, _src_id(0)
, _dst_id(0)
, _display_timeout(display_timeout)
, _up_bytes(0)
, _up_packs(0)
, _down_bytes(0)
, _down_packs(0)
, _start_tp(boost::chrono::steady_clock::now())
, _src_opened(false)
, _dst_opened(false)
, _started(false)
{
	_src_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_src_socket.set_option(asio::ip::udp::socket::send_buffer_size(sbuff_size));
	_src_socket.set_option(asio::ip::udp::socket::receive_buffer_size(rbuff_size));

	_dst_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_dst_socket.set_option(asio::ip::udp::socket::send_buffer_size(sbuff_size));
	_dst_socket.set_option(asio::ip::udp::socket::receive_buffer_size(rbuff_size));
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

	boost::system::error_code ec;
	_src_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
	if (ec) {
		LOG(ERR) << "channel[" << _id << "] src socket bind error: " << ec.message();
		return false;
	}
	_dst_socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
	if (ec) {
		LOG(ERR) << "channel[" << _id << "] dst socket bind error: " << ec.message();
		return false;
	}

	_src_local_ep = _src_socket.local_endpoint();
	_dst_local_ep = _dst_socket.local_endpoint();

	_src_ss = src_ss;
	_dst_ss = dst_ss;

	_src_id = src_ss->id();
	_dst_id = dst_ss->id();

	_src_remote_ep = asio::ip::udp::endpoint(src_ss->socket().remote_endpoint().address(), 0);
	_dst_remote_ep = asio::ip::udp::endpoint(dst_ss->socket().remote_endpoint().address(), 0);

	return true;
}

void CChannel::toStop()
{
	_src_strand.get_io_context().post(
			_src_strand.wrap(
					boost::bind(&CChannel::stop, shared_from_this())
			)
	);
	LOGF(TRACE) << "channel[" << _id << "] ("<< _src_id
			<< ") --> (" << _dst_id << ") will be stopped.";
}

void CChannel::stop()
{
	if (_started) {
		_started = false;

		SessionPtr ss = _src_ss.lock();
		if (ss) {
			ss->closeSrcChannel(shared_from_this());
		}

		ss = _dst_ss.lock();
		if (ss) {
			ss->closeDstChannel(shared_from_this());
		}

		boost::system::error_code ec;
		_src_socket.cancel(ec);
		_dst_socket.cancel(ec);
		_display_timer.cancel(ec);

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

		LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
				<< ")["    << _src_remote_ep
				<< " --> " << _src_socket.local_endpoint().port()
				<< " --> " << _dst_socket.local_endpoint().port()
				<< " --> " << _dst_remote_ep
				<< "]("    << _dst_id << ") opened.";

		asio::spawn(_src_strand, boost::bind(
				&CChannel::uploader, shared_from_this(), boost::placeholders::_1));
		asio::spawn(_dst_strand, boost::bind(
				&CChannel::downloader, shared_from_this(), boost::placeholders::_1));
		asio::spawn(_dst_strand, boost::bind(
				&CChannel::displayer, shared_from_this(), boost::placeholders::_1));
	}
}


void CChannel::uploader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
			<< ")["    << _src_remote_ep
			<< " --> " << _src_socket.local_endpoint().port()
			<< " --> " << _dst_socket.local_endpoint().port()
			<< " --> " << _dst_remote_ep
			<< "]("    << _dst_id << ") src start auth...";

	while(_started && !_src_opened) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf, 1500), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _src_remote_ep << " --> " << _src_socket.local_endpoint().port()
					<< "](" << _dst_id << ") src read auth failed: " << ec.message();
			continue;
		}

		LOG(DEBUG) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _src_remote_ep << " --> " << _src_socket.local_endpoint().port()
				<< "](" << _dst_id << ") src read auth[" << bytes << "]: "
				<< util::to_hex(_src_buf.data(), bytes);

		if (doAuth(_src_buf.data(), bytes) && _dst_opened)
			_src_opened = true;
		else
			_src_buf.clear();

		LOG(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _src_remote_ep << " <-- " << _src_socket.local_endpoint().port()
				<< "](" << _dst_id << ") src echo auth[" << bytes << "]: "
				<< util::to_hex(_src_buf.data(), bytes);

		bytes = _src_socket.async_send_to(asio::buffer(_src_buf, bytes), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			_src_opened = false;

			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _src_remote_ep << " <-- " << _src_socket.local_endpoint().port()
					<< "](" << _dst_id << ") src echo auth failed: " << ec.message();

			continue;
		}
	}

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep
			<< " --> " << _src_socket.local_endpoint().port()
			<< " --> " << _dst_socket.local_endpoint().port()
			<< " --> " << _dst_remote_ep
			<< "]("    << _dst_id << ") start upload...";

	asio::ip::udp::endpoint ep;
	while (_started) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _src_remote_ep
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload receive error: " << ec.message();
			continue;
		}

		if (ep != _src_remote_ep) {
			if (ep.address() != _src_remote_ep.address())
				continue;

			LOG(WARNING) << "channel[" << _id << "] source remote endpoint change: ["
					<< _src_remote_ep << "] ==> [" << ep << "]";

			_src_remote_ep.port(ep.port());
		}

		if (bytes <= 2)
			continue;

		bytes = _dst_socket.async_send(asio::buffer(_src_buf, bytes), yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_remote_ep
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload send error: " << ec.message();
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

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep
			<< " <-- " << _src_socket.local_endpoint().port()
			<< " <-- " << _dst_socket.local_endpoint().port()
			<< " <-- " << _dst_remote_ep
			<< "]("    << _dst_id << ") dst start auth...";

	while(_started && !_dst_opened) {
		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
					<< "](" << _dst_id << ") dst read auth error: " << ec.message();
			continue;
		}

		if (doAuth(_dst_buf.data(), bytes))
			_dst_opened = true;
		else
			_dst_buf.clear();

		LOG(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
				<< "](" << _dst_id << ") dst echo auth[" << bytes << "B]: "
				<< util::to_hex(_dst_buf.data(), bytes);

		bytes = _dst_socket.async_send_to(asio::buffer(_dst_buf, bytes), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
				<< "](" << _dst_id << ") dst echo auth[" << bytes << "B] error: " << ec.message();
		}
	}

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep
			<< " <-- " << _src_socket.local_endpoint().port()
			<< " <-- " << _dst_socket.local_endpoint().port()
			<< " <-- " << _dst_remote_ep
			<< "]("    << _dst_id << ") start download...";

	_dst_socket.connect(_dst_remote_ep, ec);
	if (ec)
		LOG(ERR) << "channel[" << _id << "] downloader connect error: " << ec.message();

	while (_started) {
		bytes = _dst_socket.async_receive(asio::buffer(_dst_buf), yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
					<< "](" << _dst_id << ") download receive error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _src_socket.async_send_to(asio::buffer(_dst_buf, bytes), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _src_remote_ep
					<< "](" << _dst_id << ") download send error: " << ec.message();
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

		LOG(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")["			<< _src_remote_ep
				<< " <--> " 	<< _dst_remote_ep
				<< "]("			<< _dst_id << ") "
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
