/*
 * CSessionMgr.cpp
 *
 *  Created on: Mar 15, 2019
 *      Author: root
 */

#include "CSessionMgr.hpp"
#include <boost/system/error_code.hpp>
#include "CServer.hpp"
#include "util/CLogger.hpp"
#include "util/CConfig.hpp"

CSessionMgr::CSessionMgr(ServerPtr server, std::string rds_addr, uint16_t rds_port, std::string rds_passwd)
: _server(server)
, _ss_db(std::ref(server->getContext()), rds_addr, rds_port, rds_passwd)
, _session_id(1000)
, _channel_id(1)
{
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::HEARTBEAT, "HEARTB"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::LOGIN,	 "LOGIN"));
	_func_name_map.insert(std::make_pair((uint32_t)FUNC::REQ::PROXY,	 "PROXY"));
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
	std::unique_lock<std::mutex> lk(_ss_mutex);
	if (_ss_map.find(id) != _ss_map.end())
		return false;

	_ss_map.insert(SessionMap::value_type(id, ss));
	return true;
}

SessionPtr CSessionMgr::getSessionWithLock(const SessionId& id)
{
	std::unique_lock<std::mutex> lk(_ss_mutex);
	auto it = _ss_map.find(id);
	if (it == _ss_map.end())
		return SessionPtr();
	return it->second.lock();
}

void CSessionMgr::closeSessionWithLock(const SessionPtr& ss)
{
	if (ss->logined()) {
		_guid_set.remove(ss->guid());
		_ss_db.del(ss->id());

		std::unique_lock<std::mutex> lk(_ss_mutex);
		_ss_map.erase(ss->id());
	}
	auto server = _server.lock();
	if (server)
		server->sessionClosed(ss);
}

bool CSessionMgr::onSessionLogin(const SessionPtr& ss)
{
	if (!_guid_set.insert(ss->guid())) {
		LOGF_ERROR << "[guid: " << ss->guid() << "] insert failed.";
		return false;
	}

	ss->id(allocSessionId());
	if (ss->type() == SESSIONTYPE::SERVER || ss->type() == SESSIONTYPE::ANYONE) {
		if (!addSessionWithLock(ss->id(), ss)) {
			LOGF_ERROR << "session id[: " << ss->id() << "] insert failed.";
			_guid_set.remove(ss->guid());
			return false;
		}

		boost::system::error_code ec;
		const auto& ep = ss->socket().remote_endpoint(ec);
		if (ec) {
			LOGF_ERROR << "get remote endpoint error: " << ec.message();
			return false;
		}

		SSessionInfo info(ss->id(), ep.address().to_v4().to_uint(), ss->guid());
		_ss_db.add(info);
	}

	return true;
}

std::shared_ptr<std::string> CSessionMgr::getAllSessions()
{
	return _ss_db.output();
}

ChannelPtr CSessionMgr::createChannel(const SessionPtr& src_ss, const SessionId& dst_id)
{
	auto server = _server.lock();
	if (!server) {
		LOGF_ERROR << "session[" << src_ss->id() << "] get server failed.";
		return ChannelPtr();
	}

	auto dst_ss = getSessionWithLock(dst_id);
	if (!dst_ss) {
		LOGF_ERROR << "session[" << src_ss->id() << "] get dest[" << dst_id << "] failed.";
		return ChannelPtr();
	}

	auto chann = std::make_shared<CChannel> (
			std::ref(server->getContext()),
			allocChannelId(),
			gConfig->channMTU(),
			gConfig->channPortExpired(),
			gConfig->channDisplayInterval()
	);

	if (!chann->init(src_ss, dst_ss)) {
		chann->stop();
		LOG_ERROR << "channel init failed!";
		return ChannelPtr();
	}
	return chann;
}
