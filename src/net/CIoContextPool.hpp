/*
 * CIoContextPool.hpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#ifndef SRC_NET_CIOCONTEXTPOOL_HPP_
#define SRC_NET_CIOCONTEXTPOOL_HPP_

#include <boost/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>

#include <vector>
#include <list>

namespace asio {
	using namespace boost::asio;
}


class CIoContextPool : private boost::noncopyable
{
public:
	CIoContextPool(size_t pool_size);
	~CIoContextPool();

	void run();
	void stop();
	asio::io_context& getIoContext();

private:
	typedef boost::shared_ptr<asio::io_context> io_context_ptr;
	typedef asio::executor_work_guard<asio::io_context::executor_type> io_context_work;

	std::vector<io_context_ptr > _ioContexts;
	std::list<io_context_work > _ioContextWorks;
	size_t _nextContextIndex;
};


#endif /* SRC_NET_CIOCONTEXTPOOL_HPP_ */
