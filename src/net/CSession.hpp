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
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/placeholders.hpp>

#include <string>
#include <deque>

namespace asio {
	using namespace boost::asio;
}

class CServer;

class CSession : public boost::enable_shared_from_this<CSession>
			   , private boost::noncopyable
{
private:
	typedef boost::shared_ptr<std::string> QueItem;
	typedef std::deque<QueItem > MsgQue;

public:
	typedef boost::shared_ptr<CSession> SelfType;
	CSession(CServer& server, asio::io_context& io_context);
	~CSession();
	void startRead();
	void write(QueItem msg);
	void stop();
	asio::ip::tcp::socket& socket() { return _socket; }

	std::string& guid() { return _guid; }
	void id(uint32_t id) { _id = id; }
	uint32_t id() { return _id; }

private:
	void onReadHead(const boost::system::error_code& ec, const size_t bytes);
	void onReadBody(const boost::system::error_code& ec, const size_t bytes);
	void writeImpl(QueItem& msg);
	void write();
	void onWriteComplete(const boost::system::error_code& ec, const size_t bytes);

	bool checkHead(SHeaderPkg& header);
	bool parseMsg(SHeaderPkg& header);
	bool packMsg();

	void onLogin(boost::shared_ptr<CReqLoginPkg>& pkg);
	void onAccelate(boost::shared_ptr<CReqAccelationPkg>& pkg);
	void onGetConsoles(boost::shared_ptr<CReqGetConsolesPkg>& pkg);

private:

	CServer& _server;
	asio::io_context::strand _strand;
	asio::ip::tcp::socket _socket;
	asio::steady_timer _timer;
	asio::streambuf _readBuf;
	MsgQue _sendQue;

	SHeaderPkg		_header;
	uint32_t 		_id;
	uint32_t		_privateAddr;
	std::string		_guid;
	bool 			_logined;
	bool			_started;
};


#endif /* SRC_NET_CSESSION_HPP_ */
