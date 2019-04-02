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
#include <boost/asio/ip/udp.hpp>
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
	typedef std::deque<StringPtr> MsgQue;

	CSession(boost::shared_ptr<CSessionMgr> mgr, asio::io_context& io_context, uint32_t timeout);
	~CSession();
	static boost::shared_ptr<CSession> newSession();
	void start();
	void toStop();
	void stop();
	bool isRuning() { return _started; }

	void doRead();
	void doWrite(const StringPtr& msg);

	std::string& guid() { return _guid; }
	bool logined() { return _logined; }
	void id(uint32_t id) { _id = id; }
	uint32_t id() { return _id; }
	asio::ip::tcp::socket& socket() { return _socket; }
	uint32_t type() { return _session_type; }
	std::string getType();

	bool addSrcChannel(const ChannelPtr& chann);
	bool addDstChannel(const ChannelPtr& chann);

	void closeSrcChannel(const ChannelPtr& chann);
	void closeDstChannel(const ChannelPtr& chann);

	void onRespAccess(const boost::shared_ptr<CReqProxyPkt>& req, ChannelPtr& chann);

private:
	void onTimeout(const boost::system::error_code& ec);
	void onReadHead(const boost::system::error_code& ec, const size_t bytes);
	void onReadBody(const boost::system::error_code& ec, const size_t bytes);
	bool checkHead();

	void onReqLogin(const boost::shared_ptr<CReqLoginPkt>& req);
	void onReqProxy(const boost::shared_ptr<CReqProxyPkt>& req);
	void onRespProxyOk(const boost::shared_ptr<CReqProxyPkt>& req, const ChannelPtr& chann);
	void onRespProxyErr(const boost::shared_ptr<CReqProxyPkt>& req, uint8_t errcode);
	void onReqGetProxies(const boost::shared_ptr<CReqGetProxiesPkt>& req);
	void onRespStopProxy(uint32_t id, const asio::ip::udp::endpoint& ep);

	void writeImpl(const StringPtr& msg);
	void write();
	void onWriteComplete(const boost::system::error_code& ec, const size_t bytes);

private:
	boost::shared_ptr<CSessionMgr> _mgr;
	asio::io_context::strand _strand;
	asio::ip::tcp::socket _socket;
	asio::steady_timer _timer;
	asio::streambuf _rbuf;
	MsgQue 			_sque;

	CChannelMap 	_src_channels;
	CChannelMap		_dst_channels;

	TagPktHdr		_hdr;
	uint32_t		_timeout;
	uint32_t 		_id;
	uint32_t	 	_session_type;
	uint32_t		_private_addr;
	std::string		_guid;

	bool 			_logined;
	boost::atomic<bool>	_started;
};

typedef boost::shared_ptr<CSession> SessionPtr;
#endif /* SRC_NET_CSESSION_HPP_ */
