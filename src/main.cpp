/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CLogger.hpp"
#include "CServer.hpp"
#include <iostream>
#include <string>

int main()
{
	InitLog("/usr/local/log/");
	CServer server("172.16.31.187", 10001, 2);
	server.start();

	return 0;
}


