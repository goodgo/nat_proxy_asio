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
#include "CLogger.hpp"
#include "util.hpp"

CChannel::CChannel(asio::io_context& io,
		uint32_t id,
		boost::shared_ptr<CSession> src_session,
		boost::shared_ptr<CSession> dst_session,
		asio::ip::udp::endpoint& src_ep,
		asio::ip::udp::endpoint& dst_ep,
		uint32_t display_timeout)
: _io_context(io)
, _src_strand(io)
, _dst_strand(io)
, _display_timer(io)
, _src_socket(io, src_ep)
, _dst_socket(io, dst_ep)
, _src_session(src_session)
, _dst_session(dst_session)
, _id(id)
, _src_id(src_session->id())
, _dst_id(dst_session->id())
, _display_timeout(display_timeout)
, _up_bytes(0)
, _up_packs(0)
, _down_bytes(0)
, _down_packs(0)
, _start_tp(boost::chrono::steady_clock::now())

, _src_opened(false)
, _dst_opened(false)
, _started(true)
{
	_src_local_ep = _src_socket.local_endpoint();
	_dst_local_ep = _dst_socket.local_endpoint();
	_src_remote_ep = asio::ip::udp::endpoint(src_session->socket().remote_endpoint().address(), 0);
	_dst_remote_ep = asio::ip::udp::endpoint(dst_session->socket().remote_endpoint().address(), 0);
	_src_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_src_socket.set_option(asio::ip::udp::socket::send_buffer_size(4096));
	_src_socket.set_option(asio::ip::udp::socket::receive_buffer_size(4096));

	_dst_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_dst_socket.set_option(asio::ip::udp::socket::send_buffer_size(4096));
	_dst_socket.set_option(asio::ip::udp::socket::receive_buffer_size(4096));
}

CChannel::~CChannel()
{
	stop();
	LOGF(TRACE) << "channel[" << _id << "].";
}

void CChannel::toStop()
{
	_io_context.post(
			_src_strand.wrap(boost::bind(&CChannel::stop, shared_from_this()))
	);
	LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
			<< ") --> (" << _dst_id << ") will be stopped.";
}

void CChannel::stop()
{
	if (_started) {
		_started = false;

		boost::system::error_code ec;
		_display_timer.cancel(ec);

		_src_socket.shutdown(asio::ip::udp::socket::shutdown_both, ec);
		_src_socket.cancel(ec);

		_dst_socket.shutdown(asio::ip::udp::socket::shutdown_both, ec);
		_dst_socket.cancel(ec);

		if (!_src_session.expired()) {
			CSession::SelfType s = _src_session.lock();
			if (s->isRuning()) {
				s->closeSrcChannel(shared_from_this());
			}
		}

		if (!_dst_session.expired()) {
			CSession::SelfType s = _dst_session.lock();
			if (s->isRuning()) {
				s->closeDstChannel(shared_from_this());
			}
		}

		boost::chrono::duration<double> sec =
				boost::chrono::steady_clock::now() - _start_tp;
		LOG(INFO) << "channel[" << _id << "] closed. takes time: " << sec.count() << " s"
				<< " | TX packets(" << _up_packs << "): " << util::formatBytes(_up_bytes)
				<< " | RX packets(" << _down_packs << "):" << util::formatBytes(_down_bytes);
	}
}

void CChannel::start()
{
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


void CChannel::uploader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;

	LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
			<< ")["    << _src_remote_ep
			<< " --> " << _src_socket.local_endpoint().port()
			<< " --> " << _dst_socket.local_endpoint().port()
			<< " --> " << _dst_remote_ep
			<< "]("    << _dst_id << ") src start auth.";

	while(_started && !_src_opened) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _src_remote_ep << " --> " << _src_socket.local_endpoint().port()
					<< "](" << _dst_id << ") src read auth failed: " << ec.message();
			continue;
		}

		LOG(DEBUG) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _src_remote_ep << " --> " << _src_socket.local_endpoint().port()
				<< "](" << _dst_id << ") src read auth[" << bytes << "]: "
				<< util::to_hex(_src_buf, bytes);

		if (doAuth(_src_buf, bytes) && _dst_opened)
			_src_opened = true;
		else
			bzero(_src_buf, bytes);

		LOG(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _src_remote_ep << " <-- " << _src_socket.local_endpoint().port()
				<< "](" << _dst_id << ") src echo auth[" << bytes << "]: "
				<< util::to_hex(_src_buf, bytes);

		bytes = _src_socket.async_send_to(asio::buffer(_src_buf, bytes), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			_src_opened = false;

			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
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
			<< "]("    << _dst_id << ") start upload.";

	asio::ip::udp::endpoint ep;
	while (_started) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf), ep, yield[ec]);
		if (ep != _src_remote_ep) {
			if (ep.address() != _src_remote_ep.address())
				continue;

			LOG(WARNING) << "channel[" << _id << "] source remote endpoint change: ["
					<< _src_remote_ep << "] ==> [" << ep << "]";

			_src_remote_ep.port(ep.port());
		}

		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _src_remote_ep
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _dst_socket.async_send(asio::buffer(_src_buf, bytes), yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_remote_ep
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload failed: " << ec.message();
			continue;
		}

		_up_bytes.add(bytes);
		_up_packs.add(1);
		/*
		LOG(TRACE) << "channel[" << _id << "] (" << _src_id
				<< ")["    << _src_remote_ep.address().to_string()
				<< " --> " << _src_socket.local_endpoint().port()
				<< " --> " << _dst_socket.local_endpoint().port()
				<< " --> " << _dst_remote_ep.address().to_string()
				<< "]("    << _dst_id << ") upload: " << bytes << " bytes.";
		*/
	}
	LOG(TRACE) << "channel[" << _id << "] uploader exit!";
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
			<< "]("    << _dst_id << ") dst start auth.";

	while(_started && !_dst_opened) {
		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
					<< "](" << _dst_id << ") dst read auth failed: " << ec.message();
			continue;
		}

		if (doAuth(_dst_buf, bytes))
			_dst_opened = true;
		else
			bzero(_dst_buf, bytes);

		LOG(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
				<< "](" << _dst_id << ") dst echo auth[" << bytes << "]: "
				<< util::to_hex(_dst_buf, bytes);

		bytes = _dst_socket.async_send_to(asio::buffer(_dst_buf, bytes), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOG(ERR) << "channel[" << _id << "] (" << _src_id
				<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
				<< "](" << _dst_id << ") dst echo auth[" << bytes << "] failed: " << ec.message();
		}
	}

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep
			<< " <-- " << _src_socket.local_endpoint().port()
			<< " <-- " << _dst_socket.local_endpoint().port()
			<< " <-- " << _dst_remote_ep
			<< "]("    << _dst_id << ") start download.";

	_dst_socket.connect(_dst_remote_ep, ec);
	if (ec)
		LOGF(ERR) << "channel[" << _id << "] downloader connect error: " << ec.message();

	while (_started) {
		bytes = _dst_socket.async_receive(asio::buffer(_dst_buf), yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _dst_remote_ep
					<< "](" << _dst_id << ") download error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _src_socket.async_send_to(asio::buffer(_dst_buf, bytes), _src_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")[" << _dst_socket.local_endpoint().port() << " <-- " << _src_remote_ep
					<< "](" << _dst_id << ") download failed: " << ec.message();
			continue;
		}

		_down_bytes.add(bytes);
		_down_packs.add(1);
	}
	LOG(TRACE) << "channel[" << _id << "] downloader exit!";
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
				<< ")["    << _src_remote_ep
				<< " <--> " << _dst_remote_ep
				<< "]("    << _dst_id << ") "
				<< "Tx(" << formatBytes((_up_bytes - up_bytes_prev) / _display_timeout)
				<< "ps/" << (_up_packs - up_packs_prev) / _display_timeout
				<< " pps) {" << formatBytes(_up_bytes) << ", "  << _up_packs
				<< " p} | Rx(" << formatBytes((_down_bytes - down_bytes_prev) / _display_timeout)
				<< "ps/" << (_down_packs - down_packs_prev) / _display_timeout
				<< " pps) {" << formatBytes(_down_bytes) << ", " << _down_packs
				<< " p}"
				;

		up_bytes_prev = _up_bytes;
		down_bytes_prev = _down_bytes;
		up_packs_prev = _up_packs;
		down_packs_prev = _down_packs;
	}
	LOG(TRACE) << "channel[" << _id << "] displayer exit!";
}

bool CChannel::doAuth(const char* buf, const size_t bytes)
{
	if (bytes > 0)
		return true;
	return false;
}
