/*
 * CSafeSet.hpp
 *
 *  Created on: Dec 17, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CSAFESET_HPP_
#define SRC_UTIL_CSAFESET_HPP_

#include <boost/thread/mutex.hpp>
#include <boost/unordered/unordered_set.hpp>

template<typename T>
class CSafeSet
{
public:
	typedef boost::unordered_set<T> Set;
	bool insert(T& item) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_set.end() != _set.find(item))
			return false;

		_set.insert(item);
		return true;
	}
	bool remove(T& item) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_set.end() == _set.find(item))
			return false;

		_set.erase(item);
		return true;
	}

	bool has(T& item) {
		boost::mutex::scoped_lock lk(_mutex);
		if (_set.end() == _set.find(item))
			return false;
		return true;
	}

private:
	Set _set;
	boost::mutex _mutex;
};




#endif /* SRC_UTIL_CSAFESET_HPP_ */
