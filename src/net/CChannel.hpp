/*
 * CChannel.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CCHANNEL_HPP_
#define SRC_NET_CCHANNEL_HPP_

#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>
#include <utility>
#include <string>
#include <unordered_map>
#include <algorithm>

#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>

class CSession;

namespace asio {
	using namespace boost::asio;
}

class CChannel;
typedef std::shared_ptr<CChannel> ChannelPtr;
typedef std::weak_ptr<CChannel> ChannelWptr;

class CChannel : public std::enable_shared_from_this<CChannel>
{
public:
	CChannel(asio::io_context& io,
			uint32_t id,
			uint32_t mtu,
			uint32_t port_expired,
			uint32_t display_interval);

	CChannel(const CChannel&)=delete;
	CChannel& operator=(const CChannel&)=delete;
	~CChannel();

	bool init(const std::shared_ptr<CSession>& src_ss,
			const std::shared_ptr<CSession>& dst_ss);
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

	std::weak_ptr<CSession>& getDstSession() {
		return _dst_end.session();
	}

private:
	bool srcAuth(asio::yield_context& yield);
	bool dstAuth(asio::yield_context& yield);
	void uploader(asio::yield_context yield);
	void downloader(asio::yield_context yield);
	void displayer(asio::yield_context yield);
	bool doAuth(const char* buf, const size_t bytes);

private:
	//////////////////////////////////////////////////////////////
	class CEnd
	{
	public:
		CEnd(asio::io_context& io, uint32_t chann_id, std::string dir, uint32_t mtu, uint32_t port_expired);
		~CEnd();
		bool init(std::shared_ptr<CSession> ss);
		void stop();
		inline void updateTime() {
			if (_port_expired > 0)
				_update_time_t = std::time(NULL);
		}
		void portExpiredChecker(asio::yield_context yield);
		bool opened() { return _opened; }
		uint16_t localPort() { return _local_ep.port(); }
		asio::ip::udp::socket::endpoint_type remote() { return _remote_ep; }
		uint32_t sessionId() { return _owner_id; }
		std::weak_ptr<CSession>& session() { return _owner_ss; }
		void zeroBuf() { std::fill(_buf.begin(), _buf.end(), 0); }

	public:
		asio::io_context::strand _strand;
		uint32_t 				_id;
		std::string 			_dir;
		asio::ip::udp::socket 	_socket;
		asio::ip::udp::endpoint _local_ep;
		asio::ip::udp::endpoint _remote_ep;

		std::weak_ptr<CSession> _owner_ss;
		uint32_t 			_owner_id;
		std::vector<char> 	_buf;

		uint32_t 	_port_expired;
		time_t 		_update_time_t;
		bool 		_opened;
	};
	//////////////////////////////////////////////////////////////
	uint32_t _id;
	CEnd _src_end;
	CEnd _dst_end;

	asio::io_context::strand _strand;
	std::atomic<uint64_t> _up_bytes;
	std::atomic<uint64_t> _up_packs;
	std::atomic<uint64_t> _down_bytes;
	std::atomic<uint64_t> _down_packs;
	std::chrono::steady_clock::time_point _start_tp;

	asio::steady_timer	_display_timer;
	uint32_t 			_display_interval;

	std::atomic<bool> _started;
};



class CChannelMap
{
public:
	typedef uint32_t	ChannelId;
	typedef std::weak_ptr<CChannel> CChannelWptr;
	typedef std::unordered_map<ChannelId, CChannelWptr> Map;
	typedef Map::value_type ValueType;
	typedef Map::iterator Iterator;

	size_t size() {
		std::unique_lock<std::mutex> lk(_mutex);
		return _map.size();
	}

	bool insert(const ChannelId& id, const ChannelPtr& value) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_map.end() != _map.find(id))
			return false;

		_map.insert(std::pair<ChannelId, CChannelWptr>(id, value));
		return true;
	}

	bool remove(const ChannelId& id) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_map.end() == _map.find(id))
			return false;

		_map.erase(id);
		return true;
	}

	bool has(const ChannelId& id) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_map.end() == _map.find(id)) {
			return false;
		}
		return true;
	}

	void stopAll() {
		std::unique_lock<std::mutex> lk(_mutex);
		for (auto it = _map.begin(); it != _map.end();) {
			auto& value = it->second;
			auto chann = value.lock();
			if (chann)
				chann->toStop();
			it = _map.erase(it);
		}
	}

private:
	mutable std::mutex _mutex;
	Map _map;
};


#endif /* SRC_NET_CCHANNEL_HPP_ */
