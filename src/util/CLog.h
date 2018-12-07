#ifndef UTILITY_LOG_H_
#define UTILITY_LOG_H_
#include <stdio.h>
#include <time.h>
#include <cstring>
#include <sys/stat.h>
#include <stdarg.h>
#include <fstream>
using namespace std;
namespace util{

extern const char prefix[8][20];
class LogModule
	{
	public:

		static LogModule* Instance()
		{
			static LogModule instance;
			return &instance;
		}

		enum
		{

			SYS_CRI = 0,
			SYS_IMP = 1,
			SYS_INF = 2,
			USR_CRI = 3,
			USR_IMP = 4,
			USR_INF = 5,
			DBG_INF = 6,
			USR_WARN = 7,
		};

		LogModule();
		~LogModule() {}

		bool onInit(const char* file, const char* logdir);
		void onRelease();

		void Log(const char* content);

		void FixLog(const char* logfile, const char* content);

		std::string getFileName(const char* logfile);

		void setBackLog(bool val) { _backlog = val; }
		void setLogSwitch(int level, bool val)
		{
			if (level >= 0 && level < 8)
				_logSwitch[level] = val;
		}

		bool isLevelOpen(int level) const
		{
			if (level >= 0 && level < 8)
				return _logSwitch[level];
			return false;
		}

	private:
		bool isDirectory(const char* path);
	private:
		bool _logSwitch[8];
		bool _backlog;
		std::string _path;
		std::string _logdir;
	};


template <int Level>
	class Logger
	{
	public:
		Logger(const char* file, const char* function, int lineno, pid_t pid)
			: _file(file)
			, _function(function)
			, _lineno(lineno)
			, _pid(pid)
		{

		}

		~Logger() {}

		void operator()(const char* fmt, ...)
		{
			if (!LogModule::Instance()->isLevelOpen(Level)) return;

			time_t tNow = time(NULL);
			//tm* lt = localtime(&tNow);
			struct tm lt={0};
			localtime_r(&tNow, &lt);
		    struct timeb tb;
		    ftime(&tb);
			char buf[2048] = {0};
			int prefixLen = snprintf(buf, 2037, "%s[%02d:%02d:%02d.%03d][pid:%ld]%s:%s[%d] >> ",
					                            prefix[Level],
					                            lt.tm_hour,
					                            lt.tm_min,
					                            lt.tm_sec,
												tb.millitm,
												(long)_pid,
					                            _file,
					                            _function,
					                            _lineno);

			int leftLen = (prefixLen < 2047) ? (2047 - prefixLen) : 0;

			va_list ap;
			va_start(ap, fmt);
			vsnprintf(buf + prefixLen, leftLen, fmt, ap);
			va_end(ap);

			LogModule::Instance()->Log(buf);
		}

	private:
		const char* _file;
		const char* _function;
		int _lineno;
		pid_t _pid;
	};


};
#define USERWARN util::Logger<util::LogModule::USR_WARN>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define DEBUGINFO util::Logger<util::LogModule::DBG_INF>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define USERINFO util::Logger<util::LogModule::USR_INF>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define USERIMPORTENT util::Logger<util::LogModule::USR_IMP>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define USERCRITIAL util::Logger<util::LogModule::USR_CRI>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define SYSTEMINFO util::Logger<util::LogModule::SYS_INF>(__FILE__, __FUNCTION__, __LINE__, GETPID)
#define gLogModule (util::LogModule::Instance())
#endif /* UTILITY_LOG_H_ */
