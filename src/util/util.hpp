/*
 * util.hpp
 *
 *  Created on: Dec 10, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_UTIL_HPP_
#define SRC_UTIL_UTIL_HPP_


#include <string>
#include <iomanip>
#include <sstream>
#include <iostream>

extern "C"
{
#include <stdint.h>
#include <sys/syscall.h>
}
#define gettid() syscall(SYS_gettid)

const uint64_t KB = 1024;
const uint64_t MB = 1024 * KB;
const uint64_t GB = 1024 * MB;
const uint64_t TB = 1024 * GB;

namespace util {
	template<typename T>
	inline std::string to_string(const T v);

	template<typename T, std::size_t N>
	inline std::string to_hex(T(&arr)[N]);

	inline std::string to_hex(const char*pbuf, const size_t len);
	inline std::string to_hex(std::string const& s);

	bool daemon();
	std::string formatBytes(const uint64_t& bytes);

	void printBacktrace(int sig);
}

template <typename T>
std::string util::to_string(const T v)
{
	std::ostringstream ss;
	ss << v;

	return ss.str();
}

std::string util::to_hex(const char*pbuf, const size_t len)
{
	std::ostringstream out;
	out << std::hex;
	for (size_t i = 0; i < len; i++)
		out << std::setfill('0') << std::setw(2) << (static_cast<short>(pbuf[i]) & 0xff) << " ";
	return out.str();
}

std::string util::to_hex(std::string const& s)
{
	return to_hex(s.c_str(), s.length());
}

template<typename T, std::size_t N>
std::string util::to_hex(T(&arr)[N])
{
	return util::to_hex(arr, N);
}

#endif /* SRC_UTIL_UTIL_HPP_ */
