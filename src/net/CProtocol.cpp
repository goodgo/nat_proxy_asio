/*
 * CProtocol.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#include "CProtocol.hpp"
#include "CLogger.hpp"
#include "util.hpp"

bool CReqLoginPkg::deserialize(const char* p, const size_t n)
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

bool CReqAccelationPkg::deserialize(const char* p, const size_t n)
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

bool CReqGetConsolesPkg::deserialize(const char* p, const size_t n)
{
	memcpy(&header, p, sizeof(SHeaderPkg));
	p += sizeof(SHeaderPkg);

	memcpy(&uiId, p, 4);
	return true;
}

boost::shared_ptr<std::string>
CRespLogin::serialize(const SHeaderPkg& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(SHeaderPkg) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	buf[8] = err;
	memcpy(buf + 9, &uiId, 4);

	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return msg;
}

boost::shared_ptr<std::string>
CRespAccelate::serialize(const SHeaderPkg& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(SHeaderPkg) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	buf[8] = err;
	memcpy(buf + 9, &uiUdpAddr, 4);
	memcpy(buf + 9 + 4, &usUdpPort, 2);

	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return msg;
}


boost::shared_ptr<std::string>
CRespAccess::serialize(const SHeaderPkg& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(SHeaderPkg) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	buf[8] = err;
	memcpy(buf + 9, &uiSrcId, 4);
	memcpy(buf + 9 + 4, &uiUdpAddr, 4);
	memcpy(buf + 9 + 4 + 4, &usUdpPort, 2);
	memcpy(buf + 9 + 4 + 4 + 2, &uiPrivateAddr, 4);

	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return msg;
}

boost::shared_ptr<std::string>
CRespGetClients::serialize(const SHeaderPkg& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(SHeaderPkg) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	memcpy(buf + 8, info.c_str(), info.length());

	boost::shared_ptr<std::string> msg (new std::string(buf, len));
	delete[] buf;
	return msg;
}
