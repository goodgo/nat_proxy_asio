/*
 * CIoContextPool.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */
#include "CIoContextPool.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include "CLogger.hpp"

CIoContextPool::CIoContextPool(size_t pool_size)
: _next_context_index(0)
, _worker_num(0)
, _started(true)
{
	_worker_num = boost::thread::hardware_concurrency();
	//if (_worker_num == 1)
	_worker_num = pool_size;

	for (size_t i = 0; i < _worker_num; i++) {
		io_context_ptr io = boost::make_shared<asio::io_context>();
		_io_contexts.push_back(io);
		_io_context_works.push_back(asio::make_work_guard(*io));
	}
}

CIoContextPool::~CIoContextPool()
{
	stop();
	LOGF(TRACE) << "io_context pool destroy.";
}

void CIoContextPool::run()
{
	std::vector<boost::shared_ptr<boost::thread> > threads;
	for (size_t i = 0; i < _io_contexts.size(); i++) {
		boost::shared_ptr<boost::thread> t(new boost::thread(
				boost::bind(&CIoContextPool::startThread, this, _io_contexts[i])));
		threads.push_back(t);
	}

	for (size_t i = 0; i < threads.size(); i++) {
		threads[i]->join();
	}
}

void CIoContextPool::startThread(io_context_ptr& io)
{
	LOGF(TRACE) << "context run.";
	try {
		io->run();
	}catch(const std::exception& e) {
		LOGF(FATAL) << "context crash: " << e.what();
	}
}

void CIoContextPool::stop()
{
	if (_started) {
		_started = false;

		for (size_t i = 0; i < _io_contexts.size(); i++)
			_io_contexts[i]->stop();
	}
}

asio::io_context& CIoContextPool::getIoContext()
{
	asio::io_context& io = *_io_contexts[_next_context_index];

	LOGF(INFO) << "get io context: " << _next_context_index;
	++_next_context_index;
	if (_next_context_index == _io_contexts.size())
		_next_context_index = 0;

	return io;
}
