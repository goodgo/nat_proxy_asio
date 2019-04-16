/*
 * CSafeMap.hpp
 *
 *  Created on: Dec 17, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CSAFEMAP_HPP_
#define SRC_UTIL_CSAFEMAP_HPP_

#include <mutex>
#include <memory>
#include <unordered_map.hpp>


template<typename K, typename V>
class CSafeMap
{
public:
	typedef std::shared_ptr<V> ValuePtr;
	typedef std::unordered_map<K, ValuePtr> Container;
	typedef typename Container::value_type value_type;
	typedef typename Container::iterator iterator;

	size_t size() {
		std::unique_lock<std::mutex> lk(_mutex);
		return _container.size();
	}

	bool insert(const K& key, ValuePtr value) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_container.end() != _container.find(key))
			return false;

		_container.insert(value_type(key, value));
		return true;
	}

	bool remove(const K& key) {
		std::unique_lock<std::mutex> lk(_mutex);
		iterator it = _container.find(key);
		if (_container.end() == it)
			return false;

		_container.erase(it);
		return true;
	}

	bool has(const K& key) {
		std::unique_lock<std::mutex> lk(_mutex);
		if (_container.end() == _container.find(key)) {
			return false;
		}
		return true;
	}

	ValuePtr get(const K& key) {
		std::unique_lock<std::mutex> lk(_mutex);
		return _container[key];
	}

	void removeAll() {
		std::unique_lock<std::mutex> lk(_mutex);
		for (iterator it = _container.begin(); it != _container.end(); ) {
			it = _container.erase(it);
		}
	}

private:
	Container _container;
	mutable std::mutex _mutex;
};

#endif /* SRC_UTIL_CSAFEMAP_HPP_ */
