/*
 * CProtocol.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */


#include "CProtocol.hpp"

bool CReqLoginPkg::convFromStream(const char* p, const size_t n)
{
	if (n < 5)
		return false;

	memcpy(&header, p, sizeof(SHeaderPkg));
	p += sizeof(SHeaderPkg);

	size_t len = p[0];
	if (n < len + 5)
	{
		return false;
	}

	szGuid.assign(p + 1, len);
	memcpy(&uiPrivateAddr, p + 1 + len, 4);
	return true;
}

bool CReqAccelationPkg::convFromStream(const char* p, const size_t n)
{
	if (n < 9)
		return false;

	memcpy(&header, p, sizeof(SHeaderPkg));
	p += sizeof(SHeaderPkg);

	memcpy(&uiId, p, 4);
	memcpy(&uiDstId, p + 4, 4);
	size_t len = p[8];
	if (n < len + 9)
	{
		return false;
	}
	szGameId.assign(p + 9, len);
	return true;
}

bool CReqGetConsolesPkg::convFromStream(const char* p, const size_t n)
{
	memcpy(&header, p, sizeof(SHeaderPkg));
	p += sizeof(SHeaderPkg);

	memcpy(&uiId, p, 4);
	return true;
}
