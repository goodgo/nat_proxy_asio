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

void InitLog(const std::string& log_dir);
void FinitLog();

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, boost::log::sources::severity_logger_mt<LogLevel>)

#define LOG(level)  BOOST_LOG_STREAM_SEV(logger::get(), level)
#define LOGF(level) LOG(level) << "[" << __FILE__ << ":" << __FUNCTION__ << "():" << __LINE__ << "] >> "

#endif
