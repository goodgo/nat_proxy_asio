/*
 * CSessionMgr.cpp
 *
 *  Created on: Mar 15, 2019
 *      Author: root
 */

#include "CSessionMgr.hpp"
#include "util/CLogger.hpp"

CSessionMgr::CSessionMgr(CServer& server)
: _server(server)
, _session_id(1000)
, _channel_id(0)
{
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::HEARTBEAT,	"HEARTB"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::LOGIN,		"LOGIN"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::PROXY,		"PROXY"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::GETPROXIES, "GETPROXIES"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::RESP::ACCESS, 	"ACCESS"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::RESP::STOPPROXY, "STOPPROXY"));
}

CSessionMgr::~CSessionMgr()
{
}

bool CSessionMgr::start()
{
	_session_db.start();
}

bool CSessionMgr::addSession(const SessionId& id, const CSession::SelfType& ss)
{
	boost::mutex::scoped_lock lock(_session_mutex);
	if (_session_map.find(id) != _session_map.end())
		return false;

	_session_map.insert(SessionMap::value_type(id, ss));
	return true;
}

CSession::SelfType CSessionMgr::getSession(const SessionId& id)
{
	CSession::SelfType ss;
	boost::mutex::scoped_lock lock(_session_mutex);
	Iterator it = _session_map.find(id);
	if (it == _session_map.end())
		return ss;

	ss = it->second.lock();
	return ss;
}

void CSessionMgr::delSession(const SessionId& id)
{
	boost::mutex::scoped_lock lock(_session_mutex);
	_session_map.erase(id);
}

void CSessionMgr::closeSession(CSession::SelfType ss)
{
	if (ss->logined()) {
		_guid_set.remove(ss->guid());
		_session_db.del(ss->id());
		delSession(ss->id());
	}

	_server.sessionClosed();
}

bool CSessionMgr::onSessionLogin(CSession::SelfType ss)
{
	if (!_guid_set.insert(ss->guid())) {
		LOGF(ERR) << "[guid: " << ss->guid() << "] insert failed.";
		return false;
	}

	ss->id(allocSessionId());
	if (!addSession(ss->id(), ss)) {
		LOGF(ERR) << "session id[: " << ss->id() << "] insert failed.";
		_guid_set.remove(ss->guid());
		return false;
	}

	SSessionInfo info(ss->id(), ss->remoteAddr());
	_session_db.add(info);

	return true;
}

boost::shared_ptr<std::string> CSessionMgr::getAllSessions(CSession::SelfType sess)
{
	return _session_db.output();
}

CChannel::SelfType CSessionMgr::createChannel(CSession::SelfType src_ss, const SessionId& id)
{
	CSession::SelfType dst_ss = getSession(id);
	if (!dst_ss) {
		LOGF(ERR) << "session[" << sess->id() << "] get dest[" << id << " ] failed.";
		return sess;
	}
	return ss;

	asio::ip::address addr = _acceptor.local_endpoint().address();
	asio::ip::udp::endpoint src_ep(addr, 0);
	asio::ip::udp::endpoint dst_ep(addr, 0);

	CChannel::SelfType chann(new CChannel(
			_io_context_pool.getIoContext(),
			allocChannelId(),
			src, dst,
			src_ep, dst_ep,
			gConfig->channReceiveBuffSize(),
			gConfig->channSendBuffSize(),
			gConfig->channDisplayTimeout()));

	src->addSrcChannel(chann);
	dst->addDstChannel(chann);

	return chann;
}


