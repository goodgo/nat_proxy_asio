#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread/thread.hpp>
#include <boost/filesystem.hpp>

#include <boost/log/core.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
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
#include <CLogger.hpp>
#include <fstream>
#include <string>

namespace logging = boost::log;
namespace attrs = boost::log::attributes;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;
namespace keywords = boost::log::keywords;

void initLog(const std::string& path) {
	
	boost::filesystem::path log_path(path);

	if (!boost::filesystem::exists(log_path))
		boost::filesystem::create_directories(log_path);

	boost::shared_ptr<logging::core> core = logging::core::get();

	boost::shared_ptr<sinks::text_file_backend> file_backend = boost::make_shared<sinks::text_file_backend>();
	file_backend->set_open_mode(std::ios::app);
	file_backend->set_file_name_pattern(log_path.string() + "/%Y%m%d_%N.log");
	file_backend->set_rotation_size(20 * 1024 * 1024);
	//file_backend->set_time_based_rotation();
	//file_backend->time_based_rotation_predicate(0);
	file_backend->auto_flush(true);

	typedef sinks::synchronous_sink<sinks::text_file_backend> FileSink;
	boost::shared_ptr<FileSink> file_sink = boost::make_shared< FileSink >(file_backend);
	file_sink->set_formatter (
		expr::format("%1% <TID:%2%>[Level:%3%] >> %4%")
		% expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S")
		% expr::attr<attrs::current_thread_id::value_type>("ThreadID")
		% expr::attr<LogLevel>("Severity")
		% expr::smessage
	);
	file_sink->set_filter(expr::attr<LogLevel>("Severity") >= TRACE);

	core->add_sink(file_sink);
	core->add_global_attribute("Scopes", attrs::named_scope());
	logging::add_common_attributes();
}

void finitLog()
{
	boost::log::core::get()->remove_all_sinks();
}
