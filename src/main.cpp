/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CLogger.hpp"
#include "CServer.hpp"
#include "util.hpp"

int main(int argc, char* argv[])
{
	if (!util::daemon()) {
		std::cerr << "daemon failed." << std::endl;
		return 0;
	}

	try {
		InitLog("/usr/local/log/");
		CServer server("0.0.0.0", 10001, 4);
		server.start();
	}
	catch(std::exception& e) {
		std::cerr << "e: " << e.what() << std::endl;
	}

	return 0;
}
