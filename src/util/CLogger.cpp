#include "CLogger.hpp"
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/support/exception.hpp>
#include <boost/exception/all.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/expressions/formatters.hpp>

CLogger::~CLogger()
{
	LOG_TRACE << "stop logger..";
	boost::log::core::get()->remove_sink(_sink);
	_sink->flush();
	_sink.reset();
}

CLogger* CLogger::getInstance()
{
	static CLogger _this;
	return &_this;
}

bool CLogger::Init(const std::string& proc_name, const std::string& path, size_t rotation_size, uint8_t level)
{
	_proc_name = proc_name;
	_path = path;
	_rotation_size = rotation_size;

	if (!generateLogFileNameFormat())
		return false;

	auto file_backend = boost::make_shared<sinks::text_file_backend>(
			keywords::open_mode = std::ios::app,
			keywords::file_name = _file_name_format,
			keywords::rotation_size = _rotation_size * 1024 * 1024,
			keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0),
			keywords::min_free_space = 30 * 1024 * 1024,
			keywords::enable_final_rotation = false,
			keywords::auto_flush = true
	);

	_sink = boost::make_shared<FileSink>(file_backend);
	_sink->set_formatter (
		expr::format("%1%_#%2%: %3%")
		% expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y%m%d_%H:%M:%S.%f")
		% expr::attr<LEVEL>("Severity")
		% expr::smessage
	);
	setLevel(level);

	auto core = logging::core::get();
	core->add_sink(_sink);
	core->add_global_attribute("Scopes", attrs::named_scope());
	logging::add_common_attributes();

	return true;
}

void CLogger::setLevel(uint8_t level)
{
	_level = level;
	if (_sink) {
		_sink->set_filter(expr::attr<LEVEL>("Severity") >= (LEVEL)_level);
	}
}

bool CLogger::generateLogFileNameFormat()
{
	boost::filesystem::path log_path(_path);

	if (!boost::filesystem::exists(log_path)) {
		boost::filesystem::create_directories(log_path);
		if (!boost::filesystem::exists(log_path))
			return false;
	}

	_file_name_format = log_path.string() + "/"
			          + _proc_name + "_" + std::to_string(gettid()) + "-%Y%m%d_%02N.log";
	return true;
}

template< typename CharT, typename TraitsT >
std::basic_ostream< CharT, TraitsT >& operator<< (
  std::basic_ostream< CharT, TraitsT >& strm, CLogger::LEVEL level)
{
	static const char* const levels[] = {
			"TRACE",
			"DEBUG",
			"INFO_",
			"WARN_",
			"ERROR",
			"FATAL",
	};
	if (static_cast<std::size_t>(level) < (sizeof(levels) / sizeof(*levels)))
		strm << gettid() << "_" << levels[level];
	else
		strm << static_cast<int>(level);
	return strm;
}

