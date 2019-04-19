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
#include <atomic>
#include <memory>

#include <boost/asio/spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/system/error_code.hpp>

#include "CProtocol.hpp"
#include "CChannel.hpp"
#include "util/CSafeMap.hpp"
#include "util/util.hpp"

namespace asio {
	using namespace boost::asio;
}

class CSessionMgr;

typedef std::shared_ptr<CSession> SessionPtr;
class CSession : public std::enable_shared_from_this<CSession>
{
public:
	typedef std::deque<StringPtr> MsgQue;

	static std::shared_ptr<CSession> newSession(std::shared_ptr<CSessionMgr>& mgr,
			asio::io_context& io_context,
			uint32_t timeout);

	CSession(std::shared_ptr<CSessionMgr> mgr, asio::io_context& io_context, uint32_t timeout);
	CSession(const CSession&) = delete;
	CSession& operator=(const CSession&) = delete;
	~CSession();

	void start();
	void toStop();
	void stop();
	bool isRuning() { return _started; }
	void reader(asio::yield_context yield);
	void asyncWrite(const StringPtr& msg);

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

	void onRespAccess(const std::shared_ptr<CReqProxyPkt>& req, const ChannelPtr& chann);

private:
	void asyncWrite();
	void onReadHead(const boost::system::error_code& ec, const size_t bytes);
	void onReadBody(const boost::system::error_code& ec, const size_t bytes);
	bool checkHead();

	void onReqLogin(const std::shared_ptr<CReqLoginPkt>& req);
	void onReqProxy(const std::shared_ptr<CReqProxyPkt>& req);
	void onRespProxyOk(const std::shared_ptr<CReqProxyPkt>& req, const ChannelPtr& chann);
	void onRespProxyErr(const std::shared_ptr<CReqProxyPkt>& req, uint8_t errcode);
	void onReqGetProxies(const std::shared_ptr<CReqGetProxiesPkt>& req);
	void onRespStopProxy(uint32_t id, const asio::ip::udp::endpoint& ep);

	void writeImpl(const StringPtr& msg);
	void write();
	void onWriteComplete(const boost::system::error_code& ec, const size_t bytes);

private:
	std::shared_ptr<CSessionMgr> _mgr;
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
	std::atomic<bool>	_started;
};

#endif /* SRC_NET_CSESSION_HPP_ */
