/*
 * CSession.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSESSION_HPP_
#define SRC_NET_CSESSION_HPP_

#include "CProtocol.hpp"
#include <boost/system/error_code.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/placeholders.hpp>

#include <string>
#include <deque>

#include "CChannel.hpp"
#include "CDataMap.hpp"
#include "util.hpp"

namespace asio {
	using namespace boost::asio;
}

class CServer;

class CSession : public boost::enable_shared_from_this<CSession>
{
public:
	typedef boost::shared_ptr<CSession> SelfType;
	typedef boost::shared_ptr<std::string> StringPtr;
	typedef std::deque<StringPtr > MsgQue;

	CSession(CServer& server, asio::io_context& io_context);
	~CSession();
	void start();
	void stop();

	void doRead();
	void doWrite(StringPtr msg);

	void id(uint32_t id) { _id = id; }
	uint32_t id() { return _id; }
	std::string& guid() { return _guid; }
	uint32_t remoteAddr() {
		return _socket.remote_endpoint().address().to_v4().to_uint();
	}
	asio::ip::tcp::socket& socket() { return _socket; }

	bool addSrcChannel(CChannel::SelfType chann) {
		 return _src_channels.insert(chann->id(), chann);
	}
	bool addDstChannel(CChannel::SelfType chann) {
		return _dst_channels.insert(chann->id(), chann);
	}

private:
	void onReadHead(const boost::system::error_code& ec, const size_t bytes);
	void onReadBody(const boost::system::error_code& ec, const size_t bytes);
	bool checkHead();

	void onLogin(boost::shared_ptr<CReqLoginPkg>& pkg);
	void onAccelate(boost::shared_ptr<CReqAccelationPkg>& pkg);
	void onGetSessions(boost::shared_ptr<CReqGetConsolesPkg>& pkg);

	void writeImpl(StringPtr msg);
	void write();
	void onWriteComplete(const boost::system::error_code& ec, const size_t bytes);

private:
	CServer& _server;
	asio::io_context::strand _strand;
	asio::ip::tcp::socket _socket;
	asio::steady_timer _timer;
	asio::streambuf _read_buf;
	MsgQue 			_send_que;

	CDataMap<uint32_t, CChannel> _src_channels;
	CDataMap<uint32_t, CChannel> _dst_channels;

	SHeaderPkg		_header;
	uint32_t 		_id;
	uint32_t		_private_addr;
	std::string		_guid;
	bool 			_logined;
	bool			_started;
};

#endif /* SRC_NET_CSESSION_HPP_ */
