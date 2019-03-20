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
#include <sstream>
#include <boost/cstdint.hpp>
#include <boost/make_shared.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

// 包头定义
typedef struct
{
	uint8_t ucHead1;	// 包头标志1
	uint8_t ucHead2;	// 包头标志2
	uint8_t ucPrtVersion; // 协议版本
	uint8_t ucSvrVersion; // 服务器版本
	uint8_t ucFunc; 	// 功能号
	uint8_t ucKeyIndex; // 密钥索引
	uint16_t usBodyLen;	// 包体长度
}TagPktHdr;

// 包头标志
namespace HEADER {
	enum {
		H1 = 0xDD,
		H2 = 0x05
	};
}

// 协议版本
namespace PROTOVERSION {
	enum {
		V1 = 0x01,
		V2 = 0x02
	};
}

// 服务器版本
namespace SVRVERSION {
	enum {
		NOENCRYP = 0x02, // 明文
		ENCRYP = 0x04 // 加密
	};
}

// 功能号
namespace FUNC {
	namespace REQ {
		enum {
			HEARTBEAT = 0x00,
			LOGIN = 0x01,
			PROXY = 0x02,
			GETPROXIES = 0x03,
		};
	}

	namespace RESP {
		enum {
			LOGIN = 0x01,
			PROXY = 0x02,
			GETPROXIES = 0x03,
			ACCESS = 0x04,
			STOPPROXY = 0x05
		};
	}
}

// 错误码
namespace ERRCODE {
	enum {
		SUCCESS = 0x00,
		REPEAT_LOGINED_ERR = 0X01,
		LOGINED_FAILED = 0X02,
		SVRVERSION_ERR = 0x03,
		PARSE_ERR = 0x04,
		CREATE_UDP_FAILED = 0x05,
	};
}

// 数据库保存Session信息
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

class CReqPktBase
{
public:
	virtual ~CReqPktBase() {};
	virtual bool deserialize(const char* p, const size_t n) = 0;
	TagPktHdr header;
};

// 登陆请求包
class CReqLoginPkt : public CReqPktBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	std::string szGuid;
	uint32_t uiPrivateAddr;
};

// 代理请求包
class CReqProxyPkt : public CReqPktBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	uint32_t uiId;
	uint32_t uiDstId;
	std::string szGameId;
};

// 获取代理端请求包
class CReqGetProxiesPkt : public CReqPktBase
{
public:
	virtual bool deserialize(const char* p, const size_t n);

public:
	uint32_t uiId;
};

/////////////////////////////////////////////////////////////////
typedef boost::shared_ptr<std::string> StringPtr;

class CRespPktBase
{
public:
	CRespPktBase() : ucErr(0) {}
	virtual ~CRespPktBase() {}
	virtual boost::shared_ptr<std::string> serialize(const TagPktHdr& head) = 0;
	virtual void error(uint8_t err) { ucErr = err; }

protected:
	virtual uint16_t size() const = 0;

protected:
	uint8_t ucErr;
};

// 登陆应答包
class CRespLogin : public CRespPktBase
{
public:
	CRespLogin()
	: CRespPktBase()
	, uiId(0)
	{}
	virtual ~CRespLogin(){}
	virtual boost::shared_ptr<std::string> serialize(const TagPktHdr& head);

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

// 请求代理应答包
class CRespProxy : public CRespPktBase
{
public:
	CRespProxy()
	: CRespPktBase()
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	{}
	virtual ~CRespProxy(){}
	virtual StringPtr serialize(const TagPktHdr& head);

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

// 接入应答包
class CRespAccess : public CRespPktBase
{
public:
	CRespAccess()
	: CRespPktBase()
	, uiSrcId(0)
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	, uiPrivateAddr(0)
	{}
	virtual ~CRespAccess(){}
	virtual StringPtr serialize(const TagPktHdr& head);

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

// 请求获取代理端应答包
class CRespGetProxies : public CRespPktBase
{
public:
	CRespGetProxies(): CRespPktBase(){}
	virtual ~CRespGetProxies(){}
	virtual StringPtr serialize(const TagPktHdr& head);

private:
	uint16_t size() const{
		return static_cast<uint16_t>(
				sessions->length());
	}
public:
	StringPtr sessions;
};

// 停止代理应答包
class CRespStopProxy : public CRespPktBase
{
public:
	CRespStopProxy()
	: CRespPktBase()
	, uiUdpId(0)
	, uiUdpAddr(0)
	, usUdpPort(0)
	{}
	virtual ~CRespStopProxy(){}
	virtual StringPtr serialize(const TagPktHdr& head);

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
