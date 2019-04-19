/*
 * CLogger.hpp
 *
 *  Created on: Dec 3, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CLOGGER_HPP_
#define SRC_UTIL_CLOGGER_HPP_

#include <string>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sinks.hpp>
#include "util/util.hpp"

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;


class CLogger
{
public:
	enum LEVEL {
		TRACE,
		DEBUG,
		INFO,
		WARN,
		ERR,
		FATAL,
	};

	typedef sinks::synchronous_sink<sinks::text_file_backend> FileSink;

	static CLogger* getInstance();
	CLogger() = default;
	CLogger(const CLogger&) = delete;
	CLogger& operator=(const CLogger&) = delete;
	~CLogger();

	bool Init(const std::string& proc_name,
			const std::string& log_dir,
			size_t rotation_size,
			uint8_t level);
	void setLevel(uint8_t level);

private:
	bool generateLogFileNameFormat();

public:
	boost::log::sources::severity_logger_mt<LEVEL> _logger;
private:
	boost::shared_ptr<FileSink> _sink;

	std::string _path;
	std::string _proc_name;
	std::string _file_name_format;
	size_t 		_rotation_size;
	uint8_t 	_level;
};

#define gLog (CLogger::getInstance())
#define LOG_TRACE BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::TRACE)
#define LOG_DEBUG BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::DEBUG)
#define LOG_INFO  BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::INFO)
#define LOG_WARN  BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::INFO)
#define LOG_ERROR BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::INFO)
#define LOG_FATAL BOOST_LOG_STREAM_SEV(gLog->_logger, CLogger::INFO)

#define FILE_FUNCTION_LINE "@" << __FILE__ << "." << __FUNCTION__ << ":" << __LINE__ << " >> "
#define LOGF_TRACE LOG_TRACE << FILE_FUNCTION_LINE
#define LOGF_DEBUG LOG_DEBUG << FILE_FUNCTION_LINE
#define LOGF_INFO  LOG_INFO  << FILE_FUNCTION_LINE
#define LOGF_WARN  LOG_WARN  << FILE_FUNCTION_LINE
#define LOGF_ERROR LOG_ERROR << FILE_FUNCTION_LINE
#define LOGF_FATAL LOG_FATAL << FILE_FUNCTION_LINE

#endif
