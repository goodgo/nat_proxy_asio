/*
 * CChannel.cpp
 *
 *  Created on: Dec 12, 2018
 *      Author: root
 */

#include "CChannel.hpp"
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include "CSession.hpp"
#include "CLogger.hpp"
#include "util.hpp"

CChannel::CChannel(asio::io_context& io,
		uint32_t id,
		boost::shared_ptr<CSession> src_session,
		boost::shared_ptr<CSession> dst_session,
		asio::ip::udp::endpoint& src_ep,
		asio::ip::udp::endpoint& dst_ep)
: _io_context(io)
, _src_strand(io)
, _dst_strand(io)
, _src_socket(io, src_ep)
, _dst_socket(io, dst_ep)
, _src_session(src_session)
, _dst_session(dst_session)
, _id(id)
, _src_id(src_session->id())
, _dst_id(dst_session->id())
, _src_opened(false)
, _dst_opened(false)
, _started(true)
{
	_src_remote_ep = asio::ip::udp::endpoint(src_session->socket().remote_endpoint().address(), 0);
	_dst_remote_ep = asio::ip::udp::endpoint(dst_session->socket().remote_endpoint().address(), 0);
	_src_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_dst_socket.set_option(asio::ip::udp::socket::reuse_address(true));

	//_src_socket.bind(_src_ep);
	//_dst_socket.bind(_dst_ep);
}

CChannel::~CChannel()
{
	stop();
	LOG(TRACE) << "channel[" << _id << "] destroy.";
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
		LOG(INFO) << "channel[" << _id << "] stopped.";
	}
}

void CChannel::start()
{
	LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
			<< ")["    << _src_remote_ep.address().to_string()
			<< " --> " << _src_socket.local_endpoint().port()
			<< " --> " << _dst_socket.local_endpoint().port()
			<< " --> " << _dst_remote_ep.address().to_string()
			<< "]("    << _dst_id << ") opened.";

	asio::spawn(_src_strand, boost::bind(&CChannel::uploader, shared_from_this(), boost::placeholders::_1));
	asio::spawn(_dst_strand, boost::bind(&CChannel::downloader, shared_from_this(), boost::placeholders::_1));
}


void CChannel::uploader(asio::yield_context yield)
{
	boost::system::error_code ec;
	asio::ip::udp::endpoint ep;
	size_t bytes = 0;
	while(_started && !_src_opened) {

		LOG(TRACE) << "channel[" << _id << "] ("<< _src_id
				<< ")["    << _src_remote_ep.address().to_string()
				<< " --> " << _src_socket.local_endpoint().port()
				<< " --> " << _dst_socket.local_endpoint().port()
				<< " --> " << _dst_remote_ep.address().to_string()
				<< "]("    << _dst_id << ") src start auth.";

		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << ep.address().to_string()
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") src auth failed: " << ec.message();
			continue;
		}

		LOGF(DEBUG) << "channel[" << _id << "] (" << _src_id
				<< ")["    << ep.address().to_string()
				<< " --> " << _src_socket.local_endpoint().port()
				<< "]("    << _dst_id << ") src read auth[ " << bytes << "]: " << util::to_hex(_src_buf, bytes);

		if ((_src_buf[0] == 0x01) && _dst_opened) {
			_src_opened = true;
			_src_remote_ep = ep;
		}
		else
			bzero(_src_buf, bytes);

		LOGF(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")["    << ep.address().to_string()
				<< " <-- " << _src_socket.local_endpoint().port()
				<< "]("    << _dst_id << ") src auth echo: " << util::to_hex(_src_buf, bytes);

		bytes = _src_socket.async_send_to(asio::buffer(_src_buf, bytes), ep, yield[ec]);
		if (ec || bytes <= 0) {
			_src_opened = false;

			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << ep.address().to_string()
					<< " <-- " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") auth echo failed: " << ec.message();
			continue;
		}
	}

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep.address().to_string()
			<< " --> " << _src_socket.local_endpoint().port()
			<< " --> " << _dst_socket.local_endpoint().port()
			<< " --> " << _dst_remote_ep.address().to_string()
			<< "]("    << _dst_id << ") start upload.";

	while (_started) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << ep.address().to_string()
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _dst_socket.async_send_to(asio::buffer(_src_buf, bytes), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_remote_ep.address()
					<< " --> " << _src_socket.local_endpoint().port()
					<< "]("    << _dst_id << ") upload error: " << ec.message();
			continue;
		}

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
	asio::ip::udp::endpoint ep;
	size_t bytes = 0;
	while(_started && !_dst_opened) {

		LOG(TRACE) << "channel[" << _id << "] (" << _src_id
				<< ")["    << _src_remote_ep.address().to_string()
				<< " <-- " << _src_socket.local_endpoint().port()
				<< " <-- " << _dst_socket.local_endpoint().port()
				<< " <-- " << _dst_remote_ep.address().to_string()
				<< "]("    << _dst_id << ") dst start auth.";

		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_socket.local_endpoint().port()
					<< " <-- " << ep.address().to_string()
					<< "]("    << _dst_id << ") dst auth failed: " << ec.message();
			continue;
		}

		if (_dst_buf[0] == 0x02) {
			_dst_opened = true;
			_dst_remote_ep = ep;
		}
		else
			bzero(_dst_buf, bytes);

		LOGF(INFO) << "channel[" << _id << "] (" << _src_id
				<< ")["    << _dst_socket.local_endpoint().port()
				<< " <-- " << ep.address().to_string()
				<< "]("    << _dst_id << ") dst auth[" << bytes << "]: " << util::to_hex(_dst_buf, bytes);

		bytes = _dst_socket.async_send_to(asio::buffer(_dst_buf, bytes), ep, yield[ec]);
		if (ec || bytes <= 0) {
			continue;
		}
	}

	LOG(TRACE) << "channel[" << _id << "] (" << _src_id
			<< ")["    << _src_remote_ep.address().to_string()
			<< " <-- " << _src_socket.local_endpoint().port()
			<< " <-- " << _dst_socket.local_endpoint().port()
			<< " <-- " << _dst_remote_ep.address().to_string()
			<< "]("    << _dst_id << ") start download.";

	while (_started) {
		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf), ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_socket.local_endpoint().port()
					<< " <-- " << ep.address().to_string()
					<< "]("    << _dst_id << ") download error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _src_socket.async_send_to(asio::buffer(_dst_buf, bytes), _dst_remote_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "channel[" << _id << "] (" << _src_id
					<< ")["    << _dst_socket.local_endpoint().port()
					<< " <-- " << _dst_remote_ep.address().to_string()
					<< "]("    << _dst_id << ") download error: " << ec.message();
			continue;
		}

		/*
		LOG(TRACE) << "channel[" << _id << "] (" << _src_id
				<< ")["    << _src_remote_ep.address().to_string()
				<< " <-- " << _src_socket.local_endpoint().port()
				<< " <-- " << _dst_socket.local_endpoint().port()
				<< " <-- " << _dst_remote_ep.address().to_string()
				<< "]("    << _dst_id << ") download: " << bytes << " bytes.";
				*/
	}
	LOG(TRACE) << "channel[" << _id << "] downloader exit!";
}
