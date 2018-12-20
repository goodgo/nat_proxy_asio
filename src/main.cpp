/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CLogger.hpp"
#include "CServer.hpp"
#include "util.hpp"

int main()
{
	InitLog("/usr/local/log/");
	/*
	if (!util::daemon()) {
		LOG(ERR) << "daemon failed.";
		FinitLog();
		return 0;
	}
	*/
	CServer server("172.16.31.192", 10001, 4);
	server.start();

	return 0;
}
