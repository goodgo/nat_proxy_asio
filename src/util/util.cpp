/*
 * util.cpp
 *
 *  Created on: Dec 20, 2018
 *      Author: root
 */

#include "util.hpp"
#include <sys/param.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>

namespace util {
	bool daemon()
	{
	 	if (signal(SIGINT, SIG_IGN) == SIG_ERR) return false;
		if (signal(SIGHUP, SIG_IGN) == SIG_ERR) return false;
		//if (signal(SIGQUIT, SIG_IGN) == SIG_ERR) return false;
		if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) return false;
		if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) return false;
		if (signal(SIGTTIN, SIG_IGN) == SIG_ERR) return false;

	    int pid = fork();
	    if (pid)
	        exit(0);
	    else if (pid < 0) {
	    	std::cerr << "fork() failed." << std::endl;
	        return false;
	    }

	    int ret = setsid();
	    if (ret < 0) {
	    	std::cerr << "setsid() failed." << std::endl;
	        return false;
	    }

	    pid = fork();
	    if (pid)
	        exit(0);
	    else if (pid < 0) {
	    	std::cerr << "fork() failed." << std::endl;
	        return false;
	    }

	    for (int i = 0; i < NOFILE; ++i)
	    	close(i);

	    ret = chdir("/");
	    if (ret < 0) {
	    	std::cerr << "chdir(\"/\") failed." << std::endl;
	        return false;
	    }

	    umask(0);
		return true;
	}

	std::string formatBytes(const uint64_t& bytes)
	{
		char buff[32] = "";
		if (bytes >= TB)
			sprintf(buff, "%.2f TB", (double)bytes / TB);
		else if (bytes >= GB)
			sprintf(buff, "%.2f GB", (double)bytes / GB);
		else if (bytes >= MB)
			sprintf(buff, "%.2f MB", (double)bytes / MB);
		else if (bytes >= KB)
			sprintf(buff, "%.2f KB", (double)bytes / KB);
		else
			sprintf(buff, "%llu B", bytes);

		return buff;
	}
}
