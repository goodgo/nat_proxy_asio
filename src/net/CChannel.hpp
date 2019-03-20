/*
 * CChannel.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CCHANNEL_HPP_
#define SRC_NET_CCHANNEL_HPP_

#include <string>

#include <boost/noncopyable.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/system/error_code.hpp>
#include <boost/chrono/chrono.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/atomic.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/chrono.hpp>

class CSession;

namespace asio {
	using namespace boost::asio;
}

class CChannel : public boost::enable_shared_from_this<CChannel>
				, public boost::noncopyable
{
public:
	CChannel(asio::io_context& io,
			uint32_t id,
			uint32_t rbuff_size = 1500,
			uint32_t sbuff_size = 1500,
			uint32_t display_timeout = 60);
	~CChannel();

	bool init(const boost::shared_ptr<CSession>& src_ss,
			const boost::shared_ptr<CSession>& dst_ss);
	void start();
	void toStop();
	void stop();

	uint32_t id() { return _id; }
	asio::ip::udp::socket::endpoint_type srcEndpoint() {
		return _src_end._local_ep;
	}

	asio::ip::udp::socket::endpoint_type dstEndpoint() {
		return _dst_end._local_ep;
	}

	boost::weak_ptr<CSession>& getDstSession() {
		return _dst_end.session();
	}

private:
	void uploader(asio::yield_context yield);
	void downloader(asio::yield_context yield);
	void displayer(asio::yield_context yield);
	bool doAuth(const char* buf, const size_t bytes);

private:
	//////////////////////////////////////////////////////////////
	class CEnd
	{
	public:
		CEnd(asio::io_context& io, uint32_t rbuff_size, uint32_t sbuff_size);
		~CEnd();
		bool init(boost::shared_ptr<CSession> ss);
		void stop();
		bool opened() { return _opened; }
		uint16_t localPort() { return _local_ep.port(); }
		asio::ip::udp::socket::endpoint_type remote() { return _remote_ep; }
		uint32_t sessionId() { return _owner_id; }
		boost::weak_ptr<CSession>& session() { return _owner_ss; }

	public:
		asio::io_context::strand _strand;
		asio::ip::udp::socket _socket;
		asio::ip::udp::endpoint _local_ep;
		asio::ip::udp::endpoint _remote_ep;
		boost::weak_ptr<CSession> _owner_ss;
		uint32_t _owner_id;
		std::vector<char> _buf;
		bool _opened;
	};
	//////////////////////////////////////////////////////////////
	uint32_t _id;
	CEnd _src_end;
	CEnd _dst_end;

	asio::io_context::strand _strand;
	asio::steady_timer _display_timer;
	uint32_t _display_timeout;
	boost::atomic<uint64_t> _up_bytes;
	boost::atomic<uint64_t> _up_packs;
	boost::atomic<uint64_t> _down_bytes;
	boost::atomic<uint64_t> _down_packs;
	boost::chrono::steady_clock::time_point _start_tp;

	boost::atomic<bool> _started;
};

typedef boost::shared_ptr<CChannel> ChannelPtr;


class CChannelMap
{
public:
	typedef uint32_t	ChannelId;
	typedef boost::weak_ptr<CChannel> CChannelWptr;
	typedef boost::unordered_map<ChannelId, CChannelWptr> Map;
	typedef Map::value_type ValueType;
	typedef Map::iterator Iterator;

	size_t size() {
		boost::mutex::scoped_lock lk(_mutex);
		return _map.size();
	}

	bool insert(const ChannelId& id, const ChannelPtr& value) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_map.end() != _map.find(id))
			return false;

		_map.insert(ValueType(id, value));
		return true;
	}

	bool remove(const ChannelId& id) {
		boost::mutex::scoped_lock lk(_mutex);
		Iterator it = _map.find(id);
		if (_map.end() == it)
			return false;

		_map.erase(id);
		return true;
	}

	bool has(const ChannelId& id) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_map.end() == _map.find(id)) {
			return false;
		}
		return true;
	}

	void stopAll() {
		boost::mutex::scoped_lock lk(_mutex);
		for (Iterator it = _map.begin(); it != _map.end();) {
			CChannelWptr& value = it->second;
			ChannelPtr chann = value.lock();
			if (chann)
				chann->toStop();
			it = _map.erase(it);
		}
	}

private:
	mutable boost::mutex _mutex;
	Map _map;
};

#endif /* SRC_NET_CCHANNEL_HPP_ */
