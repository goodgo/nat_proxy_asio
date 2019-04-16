/*
 * CIoContextPool.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */
#include "CIoContextPool.hpp"
#include <thread.hpp>
#include "util/CLogger.hpp"
#include "util/util.hpp"

CIoContextPool::CIoContextPool(size_t pool_size)
: _next_context_index(0)
, _worker_num(0)
, _started(true)
{
	_worker_num = std::thread::hardware_concurrency();
	assert(_worker_num != 0);
	_worker_num = pool_size;

	for (size_t i = 0; i < _worker_num; i++) {
		io_context_ptr io = std::make_shared<asio::io_context>();
		_io_contexts.push_back(io);
		_io_context_works.push_back(asio::make_work_guard(*io));
	}
}

CIoContextPool::~CIoContextPool()
{
	stop();
}

void CIoContextPool::run()
{
	std::vector<std::shared_ptr<std::thread> > threads;
	for (size_t i = 0; i < _io_contexts.size(); i++) {
		auto t = std::make_shared<std::thread>(
						std::bind(
								&CIoContextPool::startThread,
								this,
								std::ref(_io_contexts[i])
						)
		);
		threads.push_back(t);
	}

	for (size_t i = 0; i < threads.size(); i++) {
		if (threads[i]->joinable())
			threads[i]->join();
	}
}

void CIoContextPool::startThread(io_context_ptr& io)
{
	while (!io->stopped()) {
		try {
			LOGF(TRACE) << "context thread running. ";
			io->run();
			break;
		}catch(const std::exception& e) {
			LOGF(FATAL) << "context catch exception: " << e.what();
			util::printBacktrace(0);
			break;
		}
	}
	LOG(INFO) << "context thread exit.";
}

void CIoContextPool::stop()
{
	if (_started) {
		_started = false;

		for (std::list<io_context_work>::iterator it = _io_context_works.begin();
				it != _io_context_works.end(); ++it) {
			it->reset();
		}
		_io_context_works.clear();

		for (size_t i = 0; i < _io_contexts.size(); i++) {
			_io_contexts[i]->stop();
		}
		_io_contexts.clear();
		LOG(INFO) << "context pool stopped.";
	}
}

asio::io_context& CIoContextPool::getIoContext()
{
	asio::io_context& io = *_io_contexts[_next_context_index];

	++_next_context_index;
	if (_next_context_index == _io_contexts.size())
		_next_context_index = 0;

	return io;
}
