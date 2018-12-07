/*
 * CProtocol.hpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#ifndef SRC_NET_CPROTOCOL_HPP_
#define SRC_NET_CPROTOCOL_HPP_

#include "CInclude.hpp"

struct EN_HEAD {
enum {H1 = 0xDD, H2 = 0x05};
};

struct EN_SVR_VERSION {
enum { ENCRYP = 0x02, NOENCRYP = 0x04};
};

struct EN_FUNC {
enum {
	HEARTBEAT = 0x00,
	LOGIN = 0x01,
	ACCELATE = 0x02,
	GET_CONSOLES = 0x03,
	REQ_ACCESS = 0x04,
	STOP_ACCELATE = 0x05
}; };

typedef struct
{
	uint8_t ucHead1;
	uint8_t ucHead2;
	uint8_t ucPrtVersion;
	uint8_t ucSvrVersion;
	uint8_t ucFunc;
	uint8_t ucKeyIndex;
	uint16_t usBodyLen;
}SHeaderPkg;


class CReqPkgBase
{
public:
	bool convFromStream(const char* p, const size_t n);
	SHeaderPkg header;
};

class CReqLoginPkg : public CReqPkgBase
{
public:
	bool convFromStream(const char* p, const size_t n);

public:
	std::string szGuid;
	uint32_t uiPrivateAddr;
};


class CReqAccelationPkg : public CReqPkgBase
{
public:
	bool convFromStream(const char* p, const size_t n);

public:
	uint32_t uiId;
	uint32_t uiDstId;
	std::string szGameId;
};

class CReqGetConsolesPkg : public CReqPkgBase
{
public:
	bool convFromStream(const char* p, const size_t n);

public:
	uint32_t uiId;
};

/////////////////////////////////////////////////////////////////
class CRespPkgBase
{
public:
	uint16_t size();
	std::ostream& operator<<(std::ostream& os);
	int err;
};

class CRespLogin : public CRespPkgBase
{
public:
	uint16_t size() {
		return static_cast<uint16_t>(sizeof(uiId));
	}
	std::ostream& operator<<(std::ostream& os) {
		os << uiId;
		return os;
	}

	uint32_t uiId;
};

class CRespAccelate : public CRespPkgBase
{
public:
	uint16_t size() {
		return static_cast<uint16_t>(sizeof(uiUdpAddr) + sizeof(usUdpPort));
	}

	uint32_t uiUdpAddr;
	uint16_t usUdpPort;
};


#endif /* SRC_NET_CPROTOCOL_HPP_ */
