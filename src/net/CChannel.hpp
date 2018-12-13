/*
 * CChannel.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CCHANNEL_HPP_
#define SRC_NET_CCHANNEL_HPP_

#include <string>

#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/system/error_code.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>

class CSession;

namespace asio {
	using namespace boost::asio;
}

class CChannel : public boost::enable_shared_from_this<CChannel>
{
public:
	typedef boost::shared_ptr<CChannel> SelfType;

	CChannel(asio::io_context& io,
			boost::shared_ptr<CSession> srcSession,
			boost::shared_ptr<CSession> dstSession,
			uint32_t id, std::string ip, uint16_t port = 0);
	~CChannel();

	void start();
	void stop();

	asio::ip::udp::socket& srcSocket() { return _src_socket; }
	asio::ip::udp::socket& dstSocket() { return _dst_socket; }

	asio::ip::udp::socket::endpoint_type srcEndpoint() {
		return _src_socket.local_endpoint();
	}

	asio::ip::udp::socket::endpoint_type dstEndpoint() {
		return _dst_socket.local_endpoint();
	}

	uint32_t id() { return _id; }

private:
	void uploader(asio::yield_context yield);
	void downloader(asio::yield_context yield);

private:
	asio::io_context& _io_context;
	asio::io_context::strand _src_strand;
	asio::io_context::strand _dst_strand;
	asio::ip::udp::endpoint _src_ep;
	asio::ip::udp::endpoint _dst_ep;
	asio::ip::udp::socket _src_socket;
	asio::ip::udp::socket _dst_socket;

	boost::weak_ptr<CSession> _src_session;
	boost::weak_ptr<CSession> _dst_session;

	enum { BUFFSIZE = 1500 };
	char _src_buf[BUFFSIZE];
	char _dst_buf[BUFFSIZE];

	uint32_t _id;
	bool _src_opened;
	bool _dst_opened;
	bool _started;
};


#endif /* SRC_NET_CCHANNEL_HPP_ */
