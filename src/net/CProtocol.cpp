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
	const size_t len = sizeof(SHeaderPkg) + head.usBodyLen;
	char* buf = new char(len + 1);
	bzero(buf, len);
	memcpy(buf, &head, sizeof(head));
	buf[8] = err;
	memcpy(buf + 9, &uiId, 4);

	LOGF(TRACE) << "[login resp]: " << util::to_hex(buf, len + 1);
	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return msg;
}

boost::shared_ptr<std::string>
CRespAccelate::serialize(const SHeaderPkg& head)
{
	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>();
	return msg;
}

boost::shared_ptr<std::string>
CRespGetClients::serialize(const SHeaderPkg& head)
{
	const size_t len = sizeof(SHeaderPkg) + head.usBodyLen;
	char* buf = new char(len + 1);
	bzero(buf, len + 1);
	memcpy(buf, &head, sizeof(head));
	buf[8] = static_cast<uint8_t>(clients.size());
	for (size_t i = 0; i < clients.size(); i++) {
		memcpy(buf + 8 + (i * sizeof(SClientInfo)), &clients[i].uiId, 4);
	}

	LOGF(TRACE) << "[clients resp]: " << util::to_hex(buf, len + 1);
	boost::shared_ptr<std::string> msg = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return msg;
}
