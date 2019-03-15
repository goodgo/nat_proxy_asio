#include <boost/thread/mutex.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/atomic.hpp>
#include <sstream>
#include <string>

#include "CSessionDb.hpp"

const uint32_t DEFAULT_ID = ~0;

class CSessionMgr
{
public:
	typedef uint32_t SessionId;
	typedef boost::weak_ptr<CSession> Value;
	typedef boost::unordered_map<SessionId, Value> SessionMap;
	typedef SessionMap::iterator Iterator;
	typedef std::map<uint32_t, std::string> FuncNameMap;

	CSessionMgr(CServer& server);
	~CSessionMgr();

	bool init();
	bool start();
	bool stop();
	bool addSession(const SessionId& id, const CSession::SelfType& ss);
	CSession::SelfType getSession(const SessionId& id);
	void delSession(const SessionId& id);

	CChannel::SelfType createChannel(CSession::SelfType src, const SessionId& id);
	CSession::SelfType getSession(CSession::SelfType sess, uint32_t id);
	boost::shared_ptr<std::string> getAllSessions(CSession::SelfType sess);
	bool onSessionLogin(CSession::SelfType ss);
	void closeSession(CSession::SelfType ss);
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
	CServer& _server;
	mutable boost::mutex _session_mutex;
	SessionMap _session_map;

	std::map<uint32_t, std::string> _func_name_map;
	CSafeSet<std::string> _guid_set;
	CSessionDb _session_db;

	boost::atomic<uint32_t> _session_id;
	boost::atomic<uint32_t> _channel_id;
};
