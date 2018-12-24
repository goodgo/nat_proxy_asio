/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CLogger.hpp"
#include "CServer.hpp"
#include "util.hpp"
#include "CConfig.hpp"

int main(int argc, char* argv[])
{
	if (!gConfig->init(argc, argv))
		return 0;

	gConfig->print();
	if (gConfig->daemon()) {
		if (!util::daemon()) {
			std::cerr << "daemon failed." << std::endl;
			return 0;
		}
	}

	try {
		initLog(gConfig->logPath());
		CServer server(gConfig->workerNum());
		server.start();
	}
	catch(std::exception& e) {
		std::cerr << "e: " << e.what() << std::endl;
	}

	return 0;
}
