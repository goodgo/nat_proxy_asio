/*
 * CSafeMap.hpp
 *
 *  Created on: Dec 17, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CSAFEMAP_HPP_
#define SRC_UTIL_CSAFEMAP_HPP_

#include <boost/thread/mutex.hpp>
#include <boost/unordered/unordered_map.hpp>


template<typename K, typename V>
class CSafeMap
{
public:
	typedef boost::weak_ptr<V> ValuePtr;
	typedef boost::unordered_map<K, ValuePtr> Container;
	typedef typename Container::value_type value_type;
	typedef typename Container::iterator iterator;

	size_t size() {
		boost::mutex::scoped_lock lk(_mutex);
		return _container.size();
	}

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

#endif /* SRC_UTIL_CSAFEMAP_HPP_ */
