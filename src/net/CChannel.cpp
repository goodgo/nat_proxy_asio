/*
 * CChannel.cpp
 *
 *  Created on: Dec 12, 2018
 *      Author: root
 */

#include "CChannel.hpp"
#include <boost/system/error_code.hpp>
#include <chrono>
#include "CSession.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"

CChannel::CEnd::CEnd(asio::io_context& io, uint32_t chann_id, std::string dir,
		uint32_t mtu, uint32_t port_expired)
: _strand(io)
, _id(chann_id)
, _dir(dir)
, _socket(io, asio::ip::udp::v4())
, _owner_id(0)
, _buf(mtu)
, _port_expired(port_expired)
, _update_time_t(std::time(NULL))
, _opened(false)
{
	_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_socket.set_option(asio::ip::udp::socket::receive_buffer_size(8 * 1024 * 1024));

	updateTime();
}

CChannel::CEnd::~CEnd()
{
}

void CChannel::CEnd::stop()
{
	boost::system::error_code ignored_ec;
	_socket.close(ignored_ec);
	_owner_ss.reset();
}

bool CChannel::CEnd::init(SessionPtr ss)
{
	boost::system::error_code ec;
	auto ep = ss->socket().local_endpoint(ec);
	if (ec) {
		LOGF_ERROR << "channel[" << _id << "] " << _dir << " get local endpoint error: " << ec.message();
		return false;
	}

	_socket.bind(asio::ip::udp::endpoint(ep.address(), 0), ec);
	if (ec) {
		LOGF_ERROR << "channel[" << _id << "] " << _dir << " bind udp socket error: " << ec.message();
		return false;
	}

	_owner_ss = ss;
	_owner_id = ss->id();
	_local_ep = _socket.local_endpoint(ec);
	ep = ss->socket().remote_endpoint(ec);
	if (ec) {
		LOGF_ERROR << "channel[" << _id << "] " << _dir << " get remote endpoint error: " << ec.message();
		return false;
	}
	_remote_ep.address(ep.address());

	if (_port_expired > 0)
		asio::spawn(_strand, std::bind(&CChannel::CEnd::portExpiredChecker, this, std::placeholders::_1));
	return true;
}

void CChannel::CEnd::portExpiredChecker(asio::yield_context yield)
{
	typedef std::chrono::system_clock clock;

	boost::system::error_code ec;
	boost::asio::steady_timer timer(_strand.get_io_context());
	time_t now;

	while(_socket.is_open()) {
		now = clock::to_time_t(clock::now());
		if (now - _update_time_t >= _port_expired) {
			LOGF_ERROR << "channel[" << _id <<  "] " << _dir << " port expired. last trans time: " << std::ctime(&_update_time_t);
			stop();
			break;
		}

		timer.expires_from_now(std::chrono::seconds(10));
		timer.async_wait(yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] " << _dir << " timer error: " << ec.message();
			stop();
			continue;
		}
	}
}

CChannel::CChannel(asio::io_context& io,
		uint32_t id,
		uint32_t mtu,
		uint32_t port_expired,
		uint32_t display_interval)
: _id(id)
, _src_end(io, id, "src", mtu, port_expired)
, _dst_end(io, id, "dst", mtu, port_expired)
, _strand(io)
, _up_bytes(0)
, _up_packs(0)
, _down_bytes(0)
, _down_packs(0)
, _start_tp(std::chrono::steady_clock::now())
, _display_timer(io)
, _display_interval(display_interval)
, _started(false)
{
}

CChannel::~CChannel()
{
	stop();
	LOGF_TRACE << "channel[" << _id << "].";
}

bool CChannel::init(const SessionPtr& src_ss, const SessionPtr& dst_ss)
{
	if (!_src_end.init(src_ss)) {
		return false;
	}

	if (!_dst_end.init(dst_ss))
		return false;

	if (!src_ss->addSrcChannel(shared_from_this())) {
		LOGF_ERROR << "channel[" << _id << "] add to source session[" << src_ss->id() << "] failed!";
		return false;
	}

	if (!dst_ss->addDstChannel(shared_from_this())) {
		src_ss->closeSrcChannel(shared_from_this());
		LOGF_ERROR << "channel[" << _id << "] add to destination session[" << dst_ss->id() << "] failed!";
		return false;
	}
	return true;
}

void CChannel::toStop()
{
	_strand.get_io_context().post(
			_strand.wrap(
					std::bind(&CChannel::stop, shared_from_this())
	));
	LOG_TRACE << "channel[" << _id
			<< "]("	 << _src_end.sessionId()
			<< ") --> (" << _dst_end.sessionId()
			<< ") will be stopped.";
}

void CChannel::stop()
{
	if (_started) {
		_started = false;

		auto ss = _src_end.session().lock();
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

		auto duration = [](const std::chrono::steady_clock::time_point& tp){
			typedef std::chrono::duration<uint32_t> second_type;
			uint32_t s = std::chrono::duration_cast<second_type>(
							std::chrono::steady_clock::now() - tp).count();

			std::string hms;
			if (s >= 3600) {
				uint32_t h = s / 3600;
				s -= (3600 * h);
				hms += std::to_string(h) + "h";
			}
			if (s >= 60) {
				uint32_t m = s / 60;
				s -= (60 * m);
				hms += std::to_string(m) + "m";
			}
			hms += std::to_string(s) + "s";
			return hms;
		};
		LOG_INFO << "channel[" << _id << "] closed. takes time: {" << duration(_start_tp) << "}"
				<< " | TX packets(" << _up_packs << "): " << util::formatBytes(_up_bytes)
				<< " | RX packets(" << _down_packs << "):" << util::formatBytes(_down_bytes);
	}
}

void CChannel::start()
{
	if (!_started) {
		_started = true;

		LOG_TRACE << "channel[" << _id << "] "
				<< "("<< _src_end.sessionId()
				<< ")["    << _src_end.remote()
				<< " --> " << _src_end.localPort()
				<< " --> " << _dst_end.localPort()
				<< " --> " << _dst_end.remote()
				<< "]("    << _dst_end.sessionId() << ") opened.";

		asio::spawn(_src_end._strand,
				std::bind(&CChannel::uploader, shared_from_this(), std::placeholders::_1));
		asio::spawn(_dst_end._strand,
				std::bind(&CChannel::downloader, shared_from_this(), std::placeholders::_1));

		if (_display_interval > 0)
			asio::spawn(_strand,
					std::bind(&CChannel::displayer, shared_from_this(), std::placeholders::_1));
	}
}

bool CChannel::srcAuth(asio::yield_context& yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	while(_started && !_src_end.opened()) {
		bytes = _src_end._socket.async_receive_from(asio::buffer(_src_end._buf), _src_end._remote_ep, yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "](" 	<< _dst_end.sessionId() << ") src receive auth error: " << ec.message();

			if (!_src_end._socket.is_open()) {
				stop();
				return false;
			}
			continue;
		}
		_src_end.updateTime();

		LOG_TRACE << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _src_end.remote()
				<< " --> "	<< _src_end.localPort()
				<< "]("		<< _dst_end.sessionId()
				<< ") src read auth[" << bytes << "B]: " << util::to_hex(_src_end._buf.data(), bytes);

		if (doAuth(_src_end._buf.data(), bytes) && _dst_end.opened())
			_src_end._opened = true;
		else
			_src_end.zeroBuf();

		LOGF_INFO << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _src_end.remote()
				<< " <-- "	<< _src_end.localPort()
				<< "]("		<< _dst_end.sessionId()
				<< ") src echo auth[" << bytes << "B]: " << util::to_hex(_src_end._buf.data(), bytes);

		bytes = _src_end._socket.async_send_to(asio::buffer(_src_end._buf, bytes), _src_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			_src_end._opened = false;

			LOGF_ERROR << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " <-- "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId() << ") src echo auth[" << bytes << "B] error: " << ec.message();

			if (!_src_end._socket.is_open()) {
				stop();
				return false;
			}
			continue;
		}
	}

	LOG_TRACE << "channel[" << _id
			<< "] ("   << _src_end.sessionId()
			<< ")["    << _src_end.remote()
			<< " --> " << _src_end.localPort()
			<< " --> " << _dst_end.localPort()
			<< " --> " << _dst_end.remote()
			<< "]("    << _dst_end.sessionId() << ") start upload...";

	return true;
}

void CChannel::uploader(asio::yield_context yield)
{
	if (!srcAuth(boost::ref(yield))){
		LOGF_ERROR << "channel[" << _id << "] uploader exit!";
		return;
	}

	boost::system::error_code ec;
	size_t bytes = 0;

	asio::ip::udp::endpoint ep;
	while (_started) {
		bytes = _src_end._socket.async_receive_from(asio::buffer(_src_end._buf), ep, yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] "
					<< "(" 		<< _src_end.sessionId()
					<< ")["		<< _src_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId() << ") upload receive error: " << ec.message();

			if (!_src_end._socket.is_open()) stop();
			continue;
		}

		if (ep != _src_end.remote()) {
			if (ep.address() != _src_end.remote().address()) {
				LOGF_ERROR << "channel[" << _id << "] " << "source recv from invalid ip [" << ep << "]";
				continue;
			}

			LOG_WARN << "channel[" << _id << "] "
					<< "source remote endpoint change: [" << _src_end.remote() << "] ==> [" << ep << "]";

			_src_end._remote_ep.port(ep.port());
		}

		_src_end.updateTime();
		if (bytes <= 2) // 心跳
			continue;

		bytes = _dst_end._socket.async_send(asio::buffer(_src_end._buf, bytes), yield[ec]);
		if (ec || bytes <= 0) {
			LOGF_ERROR << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _dst_end.remote()
					<< " --> "	<< _src_end.localPort()
					<< "]("		<< _dst_end.sessionId() << ") upload send error: " << ec.message();

			if (!_dst_end._socket.is_open()) stop();
			continue;
		}

		_up_bytes.fetch_add(1);
		_up_packs.fetch_add(1);
	}
	LOGF_TRACE << "channel[" << _id << "] uploader exit!";
}

bool CChannel::dstAuth(asio::yield_context& yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	while(_started && !_dst_end._opened) {
		bytes = _dst_end._socket.async_receive_from(asio::buffer(_dst_end._buf), _dst_end._remote_ep, yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] ("
					<< _src_end.sessionId()
					<< ")["		<< _dst_end.localPort()
					<< " <-- "	<< _dst_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") dst receive auth error: " << ec.message();

			if (!_dst_end._socket.is_open()) {
				stop();
				return false;
			}
			continue;
		}

		_dst_end.updateTime();

		if (doAuth(_dst_end._buf.data(), bytes))
			_dst_end._opened = true;
		else
			_dst_end.zeroBuf();

		LOG_INFO << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _dst_end.localPort()
				<< " <-- "	<< _dst_end.remote()
				<< "]("		<< _dst_end.sessionId()
				<< ") dst echo auth[" << bytes << "B]: " << util::to_hex(_dst_end._buf.data(), bytes);

		bytes = _dst_end._socket.async_send_to(asio::buffer(_dst_end._buf, bytes), _dst_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF_ERROR << "channel[" << _id << "] "
				<< "("		<< _src_end.sessionId()
				<< ")["		<< _dst_end.localPort()
				<< " <-- "	<< _dst_end.remote()
				<< "]("		<< _dst_end.sessionId()
				<< ") dst echo auth[" << bytes << "B] error: " << ec.message();

			if (!_dst_end._socket.is_open()) {
				stop();
				return false;
			}
			continue;
		}
	}

	LOG_INFO << "channel[" << _id << "] "
			<< "(" 		<< _src_end.sessionId()
			<< ")["		<< _src_end.remote()
			<< " <-- "	<< _src_end.localPort()
			<< " <-- "	<< _dst_end.localPort()
			<< " <-- "	<< _dst_end.remote()
			<< "]("		<< _dst_end.sessionId() << ") start download...";

	return true;
}

void CChannel::downloader(asio::yield_context yield)
{
	if (!dstAuth(boost::ref(yield))) {
		LOGF_ERROR << "channel[" << _id << "] downloader exit!";
		return;
	}

	boost::system::error_code ec;
	size_t bytes = 0;

	_dst_end._socket.connect(_dst_end.remote(), ec);
	if (ec)
		LOGF_ERROR << "channel[" << _id << "] " << "downloader connect error: " << ec.message();

	while (_started) {
		bytes = _dst_end._socket.async_receive(asio::buffer(_dst_end._buf), yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")[" 	<< _dst_end.localPort()
					<< " <-- "	<< _dst_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") download receive error: " << ec.message();

			if (!_dst_end._socket.is_open()) stop();
			continue;
		}

		_dst_end.updateTime();
		if (bytes <= 2) // 心跳
			continue;

		bytes = _src_end._socket.async_send_to(asio::buffer(_dst_end._buf, bytes), _src_end._remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF_ERROR << "channel[" << _id << "] "
					<< "("		<< _src_end.sessionId()
					<< ")["		<< _dst_end.localPort()
					<< " <-- "	<< _src_end.remote()
					<< "]("		<< _dst_end.sessionId() << ") download send error: " << ec.message();

			if (!_src_end._socket.is_open()) stop();
			continue;
		}

		_down_bytes.fetch_add(bytes);
		_down_packs.fetch_add(1);
	}
	LOG_INFO << "channel[" << _id << "] downloader exit!";
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
		_display_timer.expires_from_now(std::chrono::seconds(_display_interval));
		_display_timer.async_wait(yield[ec]);
		if (ec) {
			LOGF_ERROR << "channel[" << _id << "] displayer timer error: " << ec.message();
			continue;
		}

		if (_up_packs == up_packs_prev && _down_packs == down_packs_prev)
			continue;

		LOG_INFO << "channel[" << _id << "] (" << _src_end.sessionId()
				<< ")["			<< _src_end.remote()
				<< " <--> " 	<< _dst_end.remote()
				<< "]("			<< _dst_end.sessionId() << ") "
				<< "Tx("		<< formatBytes((_up_bytes - up_bytes_prev) / _display_interval)
				<< "ps/"		<< (_up_packs - up_packs_prev) / _display_interval
				<< " pps) {"	<< formatBytes(_up_bytes) << ", "  << _up_packs
				<< " p} | Rx("	<< formatBytes((_down_bytes - down_bytes_prev) / _display_interval)
				<< "ps/"		<< (_down_packs - down_packs_prev) / _display_interval
				<< " pps) {"	<< formatBytes(_down_bytes) << ", " << _down_packs
				<< " p}"
				;

		up_bytes_prev = _up_bytes;
		down_bytes_prev = _down_bytes;
		up_packs_prev = _up_packs;
		down_packs_prev = _down_packs;
	}
	LOG_INFO << "channel[" << _id << "] displayer exit!";
}

bool CChannel::doAuth(const char* buf, const size_t bytes)
{
	if (bytes > 0)
		return true;
	return false;
}
