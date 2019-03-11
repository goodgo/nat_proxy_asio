#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
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
#include <boost/phoenix.hpp>
#include <CLogger.hpp>
#include <fstream>
#include <string>



namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

/* Define place holder attributes */
BOOST_LOG_ATTRIBUTE_KEYWORD(process_id, "ProcessID", attrs::current_process_id::value_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(thread_id, "ThreadID", attrs::current_thread_id::value_type)

// Get Process native ID
attrs::current_process_id::value_type::native_type get_native_process_id(
        logging::value_ref<attrs::current_process_id::value_type,
        tag::process_id> const& pid)
{
    if (pid)
        return pid->native_id();
    return 0;
}

// Get Thread native ID
attrs::current_thread_id::value_type::native_type get_native_thread_id(
        logging::value_ref<attrs::current_thread_id::value_type,
        tag::thread_id> const& tid)
{
    if (tid)
        return tid->native_id();
    return 0;
}


template< typename CharT, typename TraitsT >
inline std::basic_ostream< CharT, TraitsT >& operator<< (
  std::basic_ostream< CharT, TraitsT >& strm, LogLevel lvl)
{
	static const char* const str[] = {
			"TRACE",
			"DEBUG",
			"INFO ",
			"WARN ",
			"ERROR",
			"FATAL",
			"REPORT"
	};
	if (static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
		strm << str[lvl] << "] [#" << gettid();
	else
		strm << static_cast<int>(lvl);
	return strm;
}

void initLog(const std::string& proc_name, const std::string& path, size_t rotation_size, uint8_t level)
{
	boost::filesystem::path log_path(path);

	if (!boost::filesystem::exists(log_path))
		boost::filesystem::create_directories(log_path);

	boost::shared_ptr<logging::core> core = logging::core::get();

	boost::shared_ptr<sinks::text_file_backend> file_backend =
			boost::make_shared<sinks::text_file_backend>(
				keywords::open_mode = std::ios::app,
				keywords::file_name = log_path.string() + "/" + proc_name + "_%Y%m%d_%03N.log",
				keywords::rotation_size = rotation_size * 1024 * 1024,
				keywords::time_based_rotation = sinks::file::rotation_at_time_point(7, 0, 0),
				keywords::min_free_space = 30 * 1024 * 1024,
				keywords::enable_final_rotation = false,
				keywords::auto_flush = true
			);


	typedef sinks::synchronous_sink<sinks::text_file_backend> FileSink;
	boost::shared_ptr<FileSink> file_sink = boost::make_shared<FileSink>(file_backend);
	file_sink->set_formatter (
		expr::format("[P:%1%] [%2%] [%3%]> %4%")
		% boost::phoenix::bind(&get_native_process_id, process_id.or_none())
		% expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y%m%d %H:%M:%S.%f")
		% expr::attr<LogLevel>("Severity")
		% expr::smessage
	);
	file_sink->set_filter(expr::attr<LogLevel>("Severity") >= level);

	core->add_sink(file_sink);
	core->add_global_attribute("Scopes", attrs::named_scope());
	logging::add_common_attributes();
}

void finitLog()
{
	boost::log::core::get()->remove_all_sinks();
}
