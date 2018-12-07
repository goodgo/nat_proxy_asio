/*
 * CIoContextPool.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */
#include "CIoContextPool.hpp"
#include <boost/thread/thread.hpp>
#include <iostream>

CIoContextPool::CIoContextPool(size_t pool_size)
: _nextContextIndex(0)
{
	for (size_t i; i < pool_size; i++) {
		io_context_ptr io(new asio::io_context);
		_ioContexts.push_back(io);
		_ioContextWorks.push_back(asio::make_work_guard(*io));
	}
}

CIoContextPool::~CIoContextPool()
{

}

void CIoContextPool::run()
{
	std::vector<boost::shared_ptr<boost::thread> > threads;
	for (size_t i = 0; i < _ioContexts.size(); i++) {
		std::cout << "statr io_context; " << i << std::endl;

		boost::shared_ptr<boost::thread> t(new boost::thread(
				boost::bind(&asio::io_context::run, _ioContexts[i])));
		threads.push_back(t);
	}

	for (size_t i = 0; i < threads.size(); i++) {
		threads[i]->join();
	}
}

void CIoContextPool::stop()
{
	for (size_t i = 0; i < _ioContexts.size(); i++)
		_ioContexts[i]->stop();
}

asio::io_context& CIoContextPool::getIoContext()
{
	asio::io_context& io = *_ioContexts[_nextContextIndex];
	++_nextContextIndex;
	if (_nextContextIndex == _ioContexts.size())
		_nextContextIndex = 0;

	return io;
}
