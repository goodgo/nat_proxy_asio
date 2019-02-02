/*
 * CLogger.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CLOGGER_HPP_
#define SRC_UTIL_CLOGGER_HPP_

#include <string>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>

enum LogLevel {
	TRACE,
	DEBUG,
	INFO,
	WARNING,
	ERR,
	FATAL,
	REPORT
};

static const char black[] = {0x1b, '[', '1', ';', '3', '0', 'm', 0};
static const char red[] = {0x1b, '[', '1', ';', '3', '1', 'm', 0};
static const char yellow[] = {0x1b, '[', '1', ';', '3', '3', 'm', 0};
static const char blue[] = {0x1b, '[', '1', ';', '3', '4', 'm', 0};
static const char normal[] = {0x1b, '[', '0', ';', '3', '9', 'm', 0};

void initLog(const std::string& proc_name,
		const std::string& log_dir,
		size_t rotation_size,
		uint8_t level);
void finitLog();

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, boost::log::sources::severity_logger_mt<LogLevel>)

#define LOG(level)  BOOST_LOG_STREAM_SEV(logger::get(), level)
#define LOGF(level) LOG(level) << "[" << __FILE__ << ":" << __FUNCTION__ << "():" << __LINE__ << "] >> "

#endif
