#ifndef SRC_NET_CSESSIONMGR_HPP_
#define SRC_NET_CSESSIONMGR_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

#include "CSession.hpp"
#include "CChannel.hpp"
#include "CSessionDb.hpp"
#include "util/CSafeSet.hpp"

class CServer;

class CSessionMgr : public std::enable_shared_from_this<CSessionMgr>
{
public:
	typedef uint32_t SessionId;
	typedef std::weak_ptr<CSession> SessionWptr;
	typedef std::unordered_map<SessionId, SessionWptr> SessionMap;
	typedef SessionMap::iterator Iterator;
	typedef std::map<uint32_t, std::string> FuncNameMap;

	CSessionMgr(const CSessionMgr&) = delete;
	CSessionMgr& operator=(const CSessionMgr&) = delete;
	CSessionMgr(std::shared_ptr<CServer> server, std::string rds_addr, uint16_t rds_port, std::string rds_passwd);
	~CSessionMgr();

	bool init();
	bool start();
	bool stop();
	bool addSessionWithLock(const SessionId& id, const SessionPtr& ss);
	SessionPtr getSessionWithLock(const SessionId& id);
	void closeSessionWithLock(const SessionPtr& ss);

	ChannelPtr createChannel(const SessionPtr& src_ss, const SessionId& dst_id);
	std::shared_ptr<std::string> getAllSessions();
	bool onSessionLogin(const SessionPtr& ss);

	std::string getFuncName(uint8_t func)
	{
		FuncNameMap::const_iterator it = _func_name_map.find(func);
		if (it != _func_name_map.end())
			return it->second;

		std::stringstream ss("");
		ss << func;
		return ss.str();
	}

private:
	uint32_t allocSessionId() { return _session_id.fetch_add(1); }
	uint32_t allocChannelId() { return _channel_id.fetch_add(1); }

private:
	std::weak_ptr<CServer> _server;
	mutable std::mutex _ss_mutex;
	SessionMap _ss_map;
	CSessionDb _ss_db;

	std::map<uint32_t, std::string> _func_name_map;
	CSafeSet<std::string> _guid_set;

	std::atomic<uint32_t> _session_id;
	std::atomic<uint32_t> _channel_id;
};

typedef std::shared_ptr<CSessionMgr> SessionMgrPtr;

#endif
