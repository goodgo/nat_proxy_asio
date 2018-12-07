#include "CLog.h"
#include <iostream>

namespace util{

const char prefix[8][20] = {"[SYS_CRI]", "[SYS_IMP]", "[SYS_INF]", "[USR_CRI]",
			                    "[USR_IMP]", "[USR_INF]", "[DBG_INF]","[USR_WAR]"};

LogModule::LogModule()
		: _backlog(false)
	{
		for (int i = 0; i < 7; ++i)
			_logSwitch[i] = true;
	}

	bool LogModule::onInit(const char* file, const char* logdir)
	{
		_path = file;
		_logdir = logdir;
		return true;


//		CIniFile inifile;
//		if (!inifile.Load(file))
//		{
//			return false;
//		}
//
//		_path = inifile.GetStrValue("Path", section, "/usr/local/log/");
//
//		_logSwitch[SYS_CRI] = (inifile.GetIntValue("SystemCritial", section, 1) == 1);
//		_logSwitch[SYS_IMP] = (inifile.GetIntValue("SystemCImportent", section, 1) == 1);
//		_logSwitch[SYS_INF] = (inifile.GetIntValue("SystemInfo", section, 1) == 1);
//		_logSwitch[USR_CRI] = (inifile.GetIntValue("UserCritial", section, 1) == 1);
//		_logSwitch[USR_IMP] = (inifile.GetIntValue("UserImportent", section, 1) == 1);
//		_logSwitch[USR_INF] = (inifile.GetIntValue("UserInfo", section, 1) == 1);
//		_logSwitch[DBG_INF] = (inifile.GetIntValue("DebugInfo", section, 1) == 1);
//		_logSwitch[USR_WARN] = (inifile.GetIntValue("WarningInfo", section, 1) == 1);

		return true;
	}

	void LogModule::onRelease()
	{

	}

	void LogModule::Log(const char* content)
	{
		if ( _backlog)
		{

		}
		else
		{
			std::string filename = getFileName(_logdir.c_str());
			if (!filename.empty())
			{
				std::ofstream outfile(filename.c_str(), std::ios_base::out | std::ios_base::app);
				outfile << content << std::endl;
				outfile.flush();
				outfile.close();
			}
		}
	}

	void LogModule::FixLog(const char* logfile, const char* content)
	{
		std::cout << content << std::endl;
		std::string filename = getFileName(logfile);
		if (!filename.empty())
		{
			ofstream outfile(filename.c_str(), std::ios_base::out | std::ios_base::app);
			outfile << content << std::endl;
			outfile.flush();
			outfile.close();
		}
	}

	std::string LogModule::getFileName(const char* logfile)
	{
		char szPath[256];
		memset(szPath,0,sizeof(szPath));
		snprintf(szPath, 255, "%s/%s/",
				              _path.empty() ? "." : _path.c_str(),
				              logfile);
		if (!isDirectory(szPath))
		{
			if (mkdir(szPath, S_IRWXU) < 0)
			{
				return "";
			}
		}

		time_t tNow = time(NULL);
		//tm* lt = localtime(&tNow);
		struct tm lt;
		localtime_r(&tNow, &lt);
		char datebuf[100] = {0};

		sprintf(datebuf, "%s_%4d-%02d-%02d.log", "ddnat",lt.tm_year+1900, lt.tm_mon+1, lt.tm_mday);

		strcat(szPath, datebuf);
		return szPath;
	}

	bool LogModule::isDirectory(const char* path)
	{
		struct stat buf;
		if(stat(path, &buf) < 0)
			return false;
		if(S_ISDIR(buf.st_mode))
			return true;
		return false;
	}
};
