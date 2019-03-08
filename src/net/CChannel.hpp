/*
 * CChannel.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_NET_CCHANNEL_HPP_
#define SRC_NET_CCHANNEL_HPP_

#include <string>

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
{
public:
	typedef boost::shared_ptr<CChannel> SelfType;

	CChannel(asio::io_context& io,
			uint32_t id,
			boost::shared_ptr<CSession> src_session,
			boost::shared_ptr<CSession> dst_session,
			asio::ip::udp::endpoint& src_ep,
			asio::ip::udp::endpoint& dst_ep,
			uint32_t display_timeout = 60);
	~CChannel();

	void start();
	void toStop();
	void stop();

	asio::ip::udp::socket& srcSocket() { return _src_socket; }
	asio::ip::udp::socket& dstSocket() { return _dst_socket; }

	asio::ip::udp::socket::endpoint_type srcEndpoint() {
		return _src_socket.local_endpoint();
	}

	asio::ip::udp::socket::endpoint_type dstEndpoint() {
		return _dst_socket.local_endpoint();
	}

	uint32_t id() { return _id; }

private:
	void uploader(asio::yield_context yield);
	void downloader(asio::yield_context yield);
	void displayer(asio::yield_context yield);
	bool doAuth(const char* buf, const size_t bytes);

private:
	asio::io_context& _io_context;

	asio::io_context::strand _src_strand;
	asio::io_context::strand _dst_strand;
	asio::steady_timer _display_timer;

	asio::ip::udp::endpoint _src_local_ep;
	asio::ip::udp::endpoint _dst_local_ep;

	asio::ip::udp::endpoint _src_remote_ep;
	asio::ip::udp::endpoint _dst_remote_ep;

	asio::ip::udp::socket _src_socket;
	asio::ip::udp::socket _dst_socket;

	boost::weak_ptr<CSession> _src_session;
	boost::weak_ptr<CSession> _dst_session;

	enum { BUFFSIZE = 2048 };
	char _src_buf[BUFFSIZE];
	char _dst_buf[BUFFSIZE];

	uint32_t _id;
	uint32_t _src_id;
	uint32_t _dst_id;

	uint32_t _display_timeout;
	boost::atomic<uint64_t> _up_bytes;
	boost::atomic<uint64_t> _up_packs;
	boost::atomic<uint64_t> _down_bytes;
	boost::atomic<uint64_t> _down_packs;
	boost::chrono::steady_clock::time_point _start_tp;

	bool _src_opened;
	bool _dst_opened;
	boost::atomic<bool> _started;
};


class CChannelMap
{
public:
	typedef uint32_t	ChannelId;
	typedef boost::unordered_map<ChannelId, CChannel::SelfType> Map;
	typedef Map::value_type ValueType;
	typedef Map::iterator Iterator;

	size_t size() {
		boost::mutex::scoped_lock lk(_mutex);
		return _map.size();
	}

	bool insert(const ChannelId& id, CChannel::SelfType value) {
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

		_map.erase(it);
		return true;
	}

	bool has(const ChannelId& id) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_map.end() == _map.find(id)) {
			return false;
		}
		return true;
	}

	CChannel::SelfType get(const ChannelId& id) {
		boost::mutex::scoped_lock lk(_mutex);
		return _map[id];
	}

	void stopAll() {
		boost::mutex::scoped_lock lk(_mutex);
		for (Iterator it = _map.begin(); it != _map.end();) {
			it->second->toStop();
			it = _map.erase(it);
		}
	}

private:
	mutable boost::mutex _mutex;
	Map _map;
};

#endif /* SRC_NET_CCHANNEL_HPP_ */
