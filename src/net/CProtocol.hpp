/*
 * CProtocol.hpp
 *
 *  Created on: Dec 4, 2018
 *      Author: root
 */

#ifndef SRC_NET_CPROTOCOL_HPP_
#define SRC_NET_CPROTOCOL_HPP_

#include <string>
#include <vector>
#include <cstring>
#include <sstream>
#include <boost/cstdint.hpp>
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>

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

typedef struct
{
	uint32_t uiId;
}SClientInfo;

class CReqPkgBase
{
public:
	virtual ~CReqPkgBase() {};
	virtual bool deserialize(const char* p, const size_t n) = 0;
	SHeaderPkg header;
};

class CReqLoginPkg : public CReqPkgBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	std::string szGuid;
	uint32_t uiPrivateAddr;
};


class CReqAccelationPkg : public CReqPkgBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	uint32_t uiId;
	uint32_t uiDstId;
	std::string szGameId;
};

class CReqGetConsolesPkg : public CReqPkgBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	uint32_t uiId;
};

/////////////////////////////////////////////////////////////////
class CRespPkgBase
{
public:
	virtual ~CRespPkgBase() {};
	virtual uint16_t size() = 0;
	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head) = 0;

	uint8_t err;
};

class CRespLogin : public CRespPkgBase
{
public:
	uint16_t size() {
		return static_cast<uint16_t>(sizeof(uiId));
	}
	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head);

	uint32_t uiId;
};

class CRespAccelate : public CRespPkgBase
{
public:
	uint16_t size() {
		return static_cast<uint16_t>(sizeof(uiUdpAddr) + sizeof(usUdpPort));
	}

	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head);
	uint32_t uiUdpAddr;
	uint16_t usUdpPort;
};

class CRespGetClients : public CRespPkgBase
{
public:
	uint16_t size() {
		return 1 + (clients.size() * sizeof(SClientInfo));
	}
	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head);
	std::vector<SClientInfo> clients;
};


#endif /* SRC_NET_CPROTOCOL_HPP_ */
