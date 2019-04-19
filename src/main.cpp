/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include <memory>
#include <iostream>
#include <exception>
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
			std::cerr << "daemon failed.\n";
			return 0;
		}
	}

	try
	{
		CLogger::getInstance()->Init(
				gConfig->procName(),
				gConfig->logPath(),
				gConfig->logRotationSize(),
				gConfig->logPrintLevel());
		auto server = std::make_shared<CServer>(gConfig->IOWorkers());
		if (!server->start()) {
			LOGF_ERROR << "server start failed!";
			return 0;
		}
	}
	catch(std::exception& e)
	{
		std::cout << "server crash: " << e.what() << "\n";
		LOG_FATAL << "server crash: " << e.what() << "\n";
	}

	return 0;
}
