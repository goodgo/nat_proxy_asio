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
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

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

class SSessionInfo
{
public:
	SSessionInfo() : uiId(0), uiAddr(0) {}
	SSessionInfo(uint32_t id, uint32_t addr) : uiId(id), uiAddr(addr) {}
	SSessionInfo& operator=(const SSessionInfo& info)
	{
		uiId = info.uiId;
		uiAddr = info.uiAddr;
		return *this;
	}

	static uint8_t size()
	{
		return static_cast<uint8_t>(
				sizeof(uiId)+
				sizeof(uiAddr));
	}
	uint32_t uiId;
	uint32_t uiAddr;
};

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
typedef boost::shared_ptr<std::string> StringPtr;

class CRespPkgBase
{
public:
	CRespPkgBase() : ucErr(0) {}
	virtual ~CRespPkgBase() {}
	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head) = 0;
	virtual void error(uint8_t err) { ucErr = err; }

protected:
	virtual uint16_t size() const = 0;

protected:
	uint8_t ucErr;
};

class CRespLogin : public CRespPkgBase
{
public:
	CRespLogin()
	: CRespPkgBase()
	, uiId(0)
	{}
	virtual ~CRespLogin(){}
	virtual boost::shared_ptr<std::string> serialize(const SHeaderPkg& head);

	void id(uint32_t val) { uiId = val; }

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sizeof(ucErr) +
				sizeof(uiId));
	}

private:
	uint32_t uiId;
};

class CRespAccelate : public CRespPkgBase
{
public:
	CRespAccelate()
	: CRespPkgBase()
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	{}
	virtual ~CRespAccelate(){}
	virtual StringPtr serialize(const SHeaderPkg& head);

	void udpId(uint32_t val) { uiUdpId = val; }
	void udpAddr(uint32_t val) { uiUdpAddr = val; }
	void udpPort(uint16_t val) { usUdpPort = val; }

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sizeof(ucErr) +
				sizeof(uiUdpId) +
				sizeof(uiUdpAddr) +
				sizeof(usUdpPort));
	}

public:
private:
	uint32_t uiUdpId;
	uint32_t uiUdpAddr;
	uint16_t usUdpPort;
};

class CRespAccess : public CRespPkgBase
{
public:
	CRespAccess()
	: CRespPkgBase()
	, uiSrcId(0)
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	, uiPrivateAddr(0)
	{}
	virtual ~CRespAccess(){}
	virtual StringPtr serialize(const SHeaderPkg& head);

	void srcId(uint32_t val) { uiSrcId = val; }
	void udpId(uint32_t val) { uiUdpId = val; }
	void udpAddr(uint32_t val) { uiUdpAddr = val; }
	void udpPort(uint16_t val) { usUdpPort = val; }
	void privateAddr(uint32_t val) { uiPrivateAddr = val; }

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sizeof(uiSrcId) +
				sizeof(uiUdpId) +
				sizeof(uiUdpAddr) +
				sizeof(usUdpPort) +
				sizeof(uiPrivateAddr));
	}

public:
private:
	uint32_t uiSrcId;
	uint32_t uiUdpId;
	uint32_t uiUdpAddr;
	uint16_t usUdpPort;
	uint32_t uiPrivateAddr;
};

class CRespGetSessions : public CRespPkgBase
{
public:
	CRespGetSessions(): CRespPkgBase(){}
	virtual ~CRespGetSessions(){}
	virtual StringPtr serialize(const SHeaderPkg& head);

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sessions->length());
	}
public:
	StringPtr sessions;
};

class CRespStopAccelate : public CRespPkgBase
{
public:
	CRespStopAccelate()
	: CRespPkgBase()
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	{}
	virtual ~CRespStopAccelate(){}
	virtual StringPtr serialize(const SHeaderPkg& head);

	void udpId(uint32_t val) { uiUdpId = val; }
	void udpAddr(uint32_t val) { uiUdpAddr = val; }
	void udpPort(uint16_t val) { usUdpPort = val; }

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sizeof(uiUdpId) +
				sizeof(uiUdpAddr) +
				sizeof(usUdpPort)
				);
	}
public:
	uint32_t uiUdpId;
	uint32_t uiUdpAddr;
	uint16_t usUdpPort;
};
#endif /* SRC_NET_CPROTOCOL_HPP_ */
