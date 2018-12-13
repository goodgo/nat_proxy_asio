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
		CSession::SelfType srcSession,
		CSession::SelfType dstSession,
		uint32_t id, std::string ip, uint16_t port)
: _io_context(io)
, _src_strand(io)
, _dst_strand(io)
, _src_ep(asio::ip::address::from_string(ip), port)
, _dst_ep(asio::ip::address::from_string(ip), port)
, _src_socket(io)
, _dst_socket(io)
, _src_session(srcSession)
, _dst_session(dstSession)
, _id(id)
, _src_opened(false)
, _dst_opened(false)
, _started(false)
{
	CSession::SelfType src = _src_session.lock();
	_src_socket.set_option(asio::ip::udp::socket::reuse_address(true));
	_dst_socket.set_option(asio::ip::udp::socket::reuse_address(true));

	_src_socket.bind(_src_ep);
	_dst_socket.bind(_dst_ep);
}

CChannel::~CChannel()
{
	stop();
}

void CChannel::stop()
{
	if (_started) {
		_started = false;

		boost::system::error_code ec;
		_src_socket.shutdown(asio::ip::udp::socket::shutdown_both, ec);
		_dst_socket.cancel(ec);

		_dst_socket.shutdown(asio::ip::udp::socket::shutdown_both, ec);
		_src_socket.cancel(ec);

		LOGF(INFO) << "close channel: " << _id;
	}
}

void CChannel::start()
{
	_started = true;
	asio::spawn(_src_strand, boost::bind(&CChannel::uploader, shared_from_this(), boost::placeholders::_1));
	asio::spawn(_dst_strand, boost::bind(&CChannel::downloader, shared_from_this(), boost::placeholders::_1));
}

void CChannel::downloader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;
	while(!_dst_opened) {
		LOGF(TRACE) << "dst channel(" << _id << ") start read auth data.";
		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf, BUFFSIZE), _dst_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "dst channel(" << _id << ") read error: " << ec.message();
			continue;
		}
		if (_dst_buf[0] != 0x02)
			bzero(_dst_buf, bytes);
		else
			_dst_opened = true;

		LOGF(TRACE) << "dst channel(" << _id << ") read[" << bytes << "] from: "
				<< _dst_ep.address().to_string();

		bytes = _dst_socket.async_send_to(asio::buffer(_dst_buf, BUFFSIZE), _dst_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "dst channel(" << _id << ") write error: " << ec.message();
			continue;
		}
	}

	LOGF(TRACE) << "dst channel(" << _id << ") start transfer data to: "
			<< _dst_ep.address().to_string();

	while (_started) {
		bytes = _dst_socket.async_receive_from(asio::buffer(_dst_buf, BUFFSIZE), _src_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "dst channel(" << _id << ") read error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _src_socket.async_send_to(asio::buffer(_dst_buf, bytes), _dst_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "dst channel(" << _id << ") write error: " << ec.message();
			continue;
		}

		LOGF(TRACE) << "dst channel(" << _id << ") download: " << bytes;
	}
}

void CChannel::uploader(asio::yield_context yield)
{
	boost::system::error_code ec;
	size_t bytes = 0;
	while(!_src_opened) {
		LOGF(TRACE) << "src channel(" << _id << ") start read auth data.";
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf, BUFFSIZE), _src_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(ERR) << "src channel(" << _id << ") read error: " << ec.message();
			continue;
		}

		LOGF(INFO) << "src channel(" << _id << ") read[" << bytes << "]: "
				<< util::to_hex(_src_buf) << " from: " << _src_ep.address().to_string();
		if (_src_buf[0] != 0x01 || !_dst_opened)
			bzero(_src_buf, bytes);
		else
			_src_opened = true;

		bytes = _src_socket.async_send_to(asio::buffer(_src_buf, bytes), _src_ep, yield[ec]);
		if (!ec || bytes <= 0) {
			_src_opened = false;

			LOGF(ERR) << "src channel(" << _id << ") write error: " << ec.message()
				<< " to: " << _src_ep.address().to_string();
			continue;
		}

		LOGF(TRACE) << "src channel(" << _id << ") echo[" << bytes << "]: " << util::to_hex(_src_buf)
				<< " to: " << _src_ep.address().to_string();
	}

	LOGF(TRACE)  << "src channel(" << _id << ") start transfer data to: "
			<< _dst_ep.address().to_string();

	while (_started) {
		bytes = _src_socket.async_receive_from(asio::buffer(_src_buf, BUFFSIZE), _src_ep, yield[ec]);

		if (ec || bytes <= 0) {
			LOGF(TRACE) << "src channel(" << _id << ") read error: " << ec.message();
			continue;
		}

		if (bytes <= 2)
			continue;

		bytes = _dst_socket.async_send_to(asio::buffer(_src_buf, bytes), _dst_ep, yield[ec]);
		if (ec || bytes <= 0) {
			LOGF(TRACE) << "src channel(" << _id << ") write error: " << ec.message();
			continue;
		}

		LOGF(TRACE) << "src channel(" << _id << ") read transfer: " << bytes;
	}
}
