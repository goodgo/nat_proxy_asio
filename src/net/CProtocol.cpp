/*
 * CProtocol.cpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#include "CProtocol.hpp"
#include "util/CLogger.hpp"
#include "util/util.hpp"

bool CReqLoginPkt::deserialize(const char* p, const size_t n)
{
	if (n < 5)
		return false;

	memcpy(&header, p, sizeof(TagPktHdr));
	p += sizeof(TagPktHdr);

	size_t len = p[0];
	if (n < len + 5)
	{
		return false;
	}

	szGuid.assign(p + 1, len);
	memcpy(&uiPrivateAddr, p + 1 + len, 4);
	uctype = p[1+len+4];
	return true;
}

bool CReqProxyPkt::deserialize(const char* p, const size_t n)
{
	if (n < 9)
		return false;

	memcpy(&header, p, sizeof(TagPktHdr));
	p += sizeof(TagPktHdr);

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

bool CReqGetProxiesPkt::deserialize(const char* p, const size_t n)
{
	memcpy(&header, p, sizeof(TagPktHdr));
	p += sizeof(TagPktHdr);

	memcpy(&uiId, p, 4);
	return true;
}

StringPtr CRespLogin::serialize(const TagPktHdr& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(TagPktHdr) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	buf[8] = ucErr;
	memcpy(buf + 9, &uiId, 4);

	StringPtr pkt = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return pkt;
}

StringPtr CRespProxy::serialize(const TagPktHdr& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(TagPktHdr) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	buf[8] = ucErr;
	memcpy(buf + 9, &uiUdpId, 4);
	memcpy(buf + 9 + 4, &uiUdpAddr, 4);
	memcpy(buf + 9 + 4 + 4, &usUdpPort, 2);

	StringPtr pkt = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return pkt;
}


StringPtr CRespAccess::serialize(const TagPktHdr& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(TagPktHdr) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	memcpy(buf + 8, &uiSrcId, 4);
	memcpy(buf + 8 + 4, &uiUdpId, 4);
	memcpy(buf + 8 + 4 + 4, &uiUdpAddr, 4);
	memcpy(buf + 8 + 4 + 4 + 4, &usUdpPort, 2);
	memcpy(buf + 8 + 4 + 4 + 4 + 2, &uiPrivateAddr, 4);

	StringPtr pkt = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return pkt;
}

StringPtr CRespGetProxies::serialize(const TagPktHdr& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(TagPktHdr) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	memcpy(buf + 8, sessions->c_str(), sessions->length());

	StringPtr pkt = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return pkt;
}

StringPtr CRespStopProxy::serialize(const TagPktHdr& head)
{
	uint16_t bodylen = size();
	size_t len = sizeof(TagPktHdr) + bodylen;
	char* buf = new char[len + 1]();
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + 6, &bodylen, 2);
	memcpy(buf + 8, &uiUdpId, 4);
	memcpy(buf + 8 + 4, &uiUdpAddr, 4);
	memcpy(buf + 8 + 4 + 4, &usUdpPort, 2);

	StringPtr pkt = boost::make_shared<std::string>(buf, len);
	delete[] buf;
	return pkt;
}
