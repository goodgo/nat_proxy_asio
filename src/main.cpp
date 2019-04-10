/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include <iostream>
#include "net/CServer.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"
#include "util/CConfig.hpp"

int main(int argc, char* argv[])
{
	if (!gConfig->init(argc, argv)) {
		return 0;
	}

 	//signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGABRT, util::printBacktrace);
	signal(SIGTERM, util::printBacktrace);
	signal(SIGSEGV, util::printBacktrace);

	if (gConfig->daemon()) {
		if (!util::daemon()) {
			std::cout << "daemon failed." << std::endl;
			return 0;
		}
	}

	try
	{
		initLog(gConfig->procName(),
				gConfig->logPath(),
				gConfig->logRotationSize(),
				gConfig->logPrintLevel());
		ServerPtr server(boost::make_shared<CServer>(gConfig->IOWorkers()));
		if (!server->start()) {
			LOGF(ERR) << "server start failed!";
			return 0;
		}
	}
	catch(std::exception& e)
	{
		std::cout << "server crash: " << e.what() << std::endl;
	}

	return 0;
}
