/*
 * main.cpp
 *
 *  Created on: Dec 5, 2018
 *      Author: root
 */

#include "CServer.hpp"
#include <iostream>
#include <string>
#include "CInclude.hpp"
int main() {

	CServer server("172.16.31.176", 10001, 2);
	server.start();


	return 0;
}


