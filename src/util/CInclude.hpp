/*
 * CInclude.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CINCLUDE_HPP_
#define SRC_UTIL_CINCLUDE_HPP_

#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/bind.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/signal_set.hpp>

namespace asio {
	using namespace boost::asio;
};


#endif /* SRC_UTIL_CINCLUDE_HPP_ */
