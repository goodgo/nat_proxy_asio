/*
 * CSession.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CSESSION_HPP_
#define SRC_NET_CSESSION_HPP_

#include <map>
#include <deque>
#include <string>
#include <sstream>

#include <boost/system/error_code.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/placeholders.hpp>

#include "CProtocol.hpp"
#include "CChannel.hpp"
#include "util/CSafeMap.hpp"
#include "util/util.hpp"

namespace asio {
	using namespace boost::asio;
}

class CSessionMgr;


class CSession : public boost::enable_shared_from_this<CSession>
{
public:
	typedef boost::shared_ptr<CSession> SelfType;
	typedef std::deque<StringPtr > MsgQue;

	CSession(boost::shared_ptr<CSessionMgr> mgr, asio::io_context& io_context, uint32_t timeout);
	~CSession();
	void start();
	void toStop();
	void stop();
	bool isRuning() { return _started; }

	void doRead();
	void doWrite(StringPtr msg);

	std::string& guid() { return _guid; }
	bool logined() { return _logined; }
	void id(uint32_t id) { _id = id; }
	uint32_t id() { return _id; }
	uint32_t remoteAddr() {
		return _socket.remote_endpoint().address().to_v4().to_uint();
	}
	asio::ip::tcp::socket& socket() { return _socket; }

	bool addSrcChannel(CChannel::SelfType chann);
	bool addDstChannel(CChannel::SelfType chann);

	void closeSrcChannel(CChannel::SelfType chann);
	void closeDstChannel(CChannel::SelfType chann);

private:
	void onTimeout(const boost::system::error_code& ec);
	void onReadHead(const boost::system::error_code& ec, const size_t bytes);
	void onReadBody(const boost::system::error_code& ec, const size_t bytes);

	bool checkHead();

	void onReqLogin(boost::shared_ptr<CReqLoginPkt>& pkg);
	void onReqProxy(boost::shared_ptr<CReqProxyPkt>& pkg);
	void onReqGetProxies(boost::shared_ptr<CReqGetProxiesPkt>& pkg);
	void onRespStopProxy(CChannel::SelfType chann);

	void writeImpl(StringPtr msg);
	void write();
	void onWriteComplete(const boost::system::error_code& ec, const size_t bytes);

private:
	boost::shared_ptr<CSessionMgr> _mgr;
	asio::io_context::strand _strand;
	asio::ip::tcp::socket _socket;
	asio::steady_timer _timer;
	asio::streambuf _read_buf;
	MsgQue 			_send_que;

	CChannelMap 	_src_channels;
	CChannelMap		_dst_channels;

	TagPktHdr		_hdr;
	uint32_t		_timeout;
	uint32_t 		_id;
	uint32_t		_private_addr;
	std::string		_guid;

	bool 			_logined;
	boost::atomic<bool>	_started;
};

#endif /* SRC_NET_CSESSION_HPP_ */
