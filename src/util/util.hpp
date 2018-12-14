/*
 * util.hpp
 *
 *  Created on: Dec 10, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_UTIL_HPP_
#define SRC_UTIL_UTIL_HPP_

#include <string>
#include <iomanip>
#include <boost/spirit/include/karma.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

namespace util {
	inline std::string to_hex(const char*pbuf, const size_t len)
	{
		std::ostringstream out;
		out << std::hex;
		for (size_t i = 0; i < len; i++)
			out << std::setfill('0') << std::setw(2) << (static_cast<short>(pbuf[i]) & 0xff) << " ";
		return out.str();
	}

	inline std::string to_hex(std::string const& s)
	{
		return to_hex(s.c_str(), s.length());
	}

	template<typename T, std::size_t N>
	inline std::string to_hex(T(&arr)[N])
	{
		return to_hex(arr, N);
	}


	template<typename T>
	class DataSet
	{
	public:
		typedef boost::unordered_set<T> Container;
		bool insert(T& item) {
			boost::mutex::scoped_lock lk(_mutex);
			if (_container.end() != _container.find(item))
				return false;

			_container.insert(item);
			return true;
		}
		bool remove(T& item) {
			boost::mutex::scoped_lock lk(_mutex);
			if (_container.end() == _container.find(item))
				return false;

			_container.erase(item);
			return true;
		}

		bool has(T& item) {
			boost::mutex::scoped_lock lk(_mutex);
			if (_container.end() == _container.find(item))
				return false;
			return true;
		}

	private:
		Container _container;
		boost::mutex _mutex;
	};

	template<typename K, typename V>
	class DataMap
	{
	public:
		typedef boost::weak_ptr<V> ValuePtr;
		typedef boost::unordered_map<K, ValuePtr> Container;
		typedef typename Container::value_type value_type;
		typedef typename Container::iterator iterator;

		bool insert(const K& key, ValuePtr value) {
			boost::mutex::scoped_lock lk(_mutex);
			if (_container.end() != _container.find(key))
				return false;

			_container.insert(value_type(key, value));
			return true;
		}

		bool remove(const K& key) {
			boost::mutex::scoped_lock lk(_mutex);
			iterator it = _container.find(key);
			if (_container.end() == it)
				return false;

			_container.erase(it);
			return true;
		}

		bool has(const K& key) {
			boost::mutex::scoped_lock lk(_mutex);
			if (_container.end() == _container.find(key)) {
				return false;
			}
			return true;
		}

		ValuePtr get(const K& key) {
			boost::mutex::scoped_lock lk(_mutex);
			return _container[key];
		}

		void removeAll() {
			boost::mutex::scoped_lock lk(_mutex);
			iterator it = _container.begin();
			for (; it != _container.end(); ++it) {
				if (!it->second.expired()) {
					boost::shared_ptr<V> p = it->second.lock();
					p.reset();
				}
				_container.erase(it);
			}
		}

	private:
		Container _container;
		boost::mutex _mutex;
	};
}


#endif /* SRC_UTIL_UTIL_HPP_ */
