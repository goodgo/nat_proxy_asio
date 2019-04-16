/*
 * CSafeSet.hpp
 *
 *  Created on: Dec 17, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CSAFESET_HPP_
#define SRC_UTIL_CSAFESET_HPP_

#include <mutex>
#include <unordered_set>

template<typename T>
class CSafeSet
{
public:
	typedef std::unordered_set<T> Set;
	bool insert(T& item) {
		std::unique_lock<std::mutex> _mutex;
		if (_set.end() != _set.find(item))
			return false;

		_set.insert(item);
		return true;
	}
	bool remove(T& item) {
		std::unique_lock<std::mutex> _mutex;
		if (_set.end() == _set.find(item))
			return false;

		_set.erase(item);
		return true;
	}

	bool has(T& item) {
		std::unique_lock<std::mutex> _mutex;
		if (_set.end() == _set.find(item))
			return false;
		return true;
	}

private:
	Set _set;
	mutable std::mutex _mutex;
};




#endif /* SRC_UTIL_CSAFESET_HPP_ */
