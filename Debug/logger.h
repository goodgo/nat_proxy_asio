#ifndef SRC_UTIL_LOGGER_H_
#define SRC_UTIL_LOGGER_H_

#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <string>

enum LogLevel {
	TRACE,
	DEBUG,
	INFO,
	WARNING,
	ERR,
	FATAL,
	REPORT
};

void InitLog(const std::string& log_dir);
void FinitLog();

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, boost::log::sources::severity_logger_mt<LogLevel>)

#define LOG(level)  BOOST_LOG_STREAM_SEV(logger::get(), level)

#endif
