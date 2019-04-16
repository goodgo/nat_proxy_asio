/*
 * CIoContextPool.hpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#ifndef SRC_NET_CIOCONTEXTPOOL_HPP_
#define SRC_NET_CIOCONTEXTPOOL_HPP_

#include <list>
#include <vector>
#include <thread>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

namespace asio {
	using namespace boost::asio;
}


class CIoContextPool
{
	typedef std::shared_ptr<asio::io_context> io_context_ptr;
	typedef asio::executor_work_guard<asio::io_context::executor_type> io_context_work;

public:
	CIoContextPool(const CIoContextPool&) = delete;
	CIoContextPool& operator=(const CIoContextPool&) = delete;
	CIoContextPool(size_t pool_size);
	~CIoContextPool();

	void run();
	void stop();
	asio::io_context& getIoContext();
	void startThread(io_context_ptr& io);
	size_t workerNum() { return _worker_num; }

private:
	std::vector<io_context_ptr > _io_contexts;
	std::list<io_context_work > _io_context_works;
	size_t _next_context_index;
	size_t _worker_num;
	bool _started;
};


#endif /* SRC_NET_CIOCONTEXTPOOL_HPP_ */
