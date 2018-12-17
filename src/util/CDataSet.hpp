/*
 * CDataSet.hpp
 *
 *  Created on: Dec 17, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CDATASET_HPP_
#define SRC_UTIL_CDATASET_HPP_

#include <boost/thread/mutex.hpp>
#include <boost/unordered/unordered_set.hpp>

template<typename T>
class CDataSet
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




#endif /* SRC_UTIL_CDATASET_HPP_ */
