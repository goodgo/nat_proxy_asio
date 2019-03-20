/*
 * CSessionMgr.cpp
 *
 *  Created on: Mar 15, 2019
 *      Author: root
 */

#include "CSessionMgr.hpp"
#include <boost/make_shared.hpp>
#include "CServer.hpp"
#include "util/CLogger.hpp"
#include "util/CConfig.hpp"

CSessionMgr::CSessionMgr(CServer& server)
: _server(server)
, _session_id(1000)
, _channel_id(1)
{
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::HEARTBEAT,"HEARTB"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::LOGIN,	"LOGIN"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::PROXY,	"PROXY"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::GETPROXIES,"GETPROXIES"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::RESP::ACCESS, 	 "ACCESS"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::RESP::STOPPROXY,"STOPPROXY"));
}

CSessionMgr::~CSessionMgr()
{
	stop();
}

bool CSessionMgr::start()
{
	_ss_db.start();
	return true;
}

bool CSessionMgr::stop()
{
	_ss_db.stop();
	return true;
}

bool CSessionMgr::addSessionWithLock(const SessionId& id, const SessionPtr& ss)
{
	boost::mutex::scoped_lock lock(_ss_mutex);
	if (_ss_map.find(id) != _ss_map.end())
		return false;

	_ss_map.insert(SessionMap::value_type(id, ss));
	return true;
}

SessionPtr CSessionMgr::getSessionWithLock(const SessionId& id)
{
	boost::mutex::scoped_lock lock(_ss_mutex);
	Iterator it = _ss_map.find(id);
	if (it == _ss_map.end())
		return SessionPtr();
	return it->second.lock();
}

void CSessionMgr::closeSessionWithLock(const SessionPtr& ss)
{
	if (ss->logined()) {
		_guid_set.remove(ss->guid());
		_ss_db.del(ss->id());

		boost::mutex::scoped_lock lock(_ss_mutex);
		_ss_map.erase(ss->id());
	}
}

bool CSessionMgr::onSessionLogin(const SessionPtr& ss)
{
	if (!_guid_set.insert(ss->guid())) {
		LOGF(ERR) << "[guid: " << ss->guid() << "] insert failed.";
		return false;
	}

	ss->id(allocSessionId());
	if (!addSessionWithLock(ss->id(), ss)) {
		LOGF(ERR) << "session id[: " << ss->id() << "] insert failed.";
		_guid_set.remove(ss->guid());
		return false;
	}

	SSessionInfo info(ss->id(), ss->remoteAddr());
	_ss_db.add(info);

	return true;
}

boost::shared_ptr<std::string> CSessionMgr::getAllSessions()
{
	return _ss_db.output();
}

ChannelPtr CSessionMgr::createChannel(const SessionPtr& src_ss, const SessionId& dst_id)
{
	SessionPtr dst_ss = getSessionWithLock(dst_id);
	if (!dst_ss) {
		LOGF(ERR) << "session[" << src_ss->id() << "] get dest[" << dst_id << "] failed.";
		return ChannelPtr();
	}

	ChannelPtr chann = boost::make_shared<CChannel> (
			boost::ref(_server.getContext()),
			allocChannelId(),
			gConfig->channReceiveBuffSize(),
			gConfig->channSendBuffSize(),
			gConfig->channDisplayTimeout()
	);

	if (!chann->init(src_ss, dst_ss)) {
		chann->stop();
		LOG(ERR) << "channel init failed!";
		return ChannelPtr();
	}
	return chann;
}
