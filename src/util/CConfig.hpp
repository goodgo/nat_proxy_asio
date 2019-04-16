/*
 * CConfig.hpp
 *
 *  Created on: Dec 10, 2018
 *      Author: root
 */

#ifndef SRC_UTIL_CONFIG_HPP
#define SRC_UTIL_CONFIG_HPP

#include <string>
#include <iostream>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include "version.h"

extern "C" {
#include <ifaddrs.h>
}

class CConfig
{
public:
	static CConfig* getInstance()
	{
		static CConfig _this;
		return &_this;
	}

	bool init(int argc, char* argv[])
	{
		try
		{
			if (!parseCmd(argc, argv))
				return false;

			boost::property_tree::ini_parser::read_ini(_cfg_file, _cfg);

			_log_rotation_size = _cfg.get<size_t>("log.RotationSize", 100);
			_log_rotation_size = std::min<size_t>(_log_rotation_size, 100);
			_log_print_level = _cfg.get<uint8_t>("log.PrintLevel", 0);

			_srv_ip = _cfg.get<std::string>("srv.IP", "0.0.0.0");
			_srv_port = _cfg.get<uint16_t>("srv.ListenPort", 10001);
			_srv_workers = _cfg.get<uint8_t>("srv.Workers", 2);
			_srv_max_sessions = _cfg.get<uint32_t>("srv.MaxSessions", 0);
			_srv_max_channels = _cfg.get<uint32_t>("srv.MaxChannels", 0);
			_srv_per_session_max_channels = _cfg.get<uint32_t>("srv.PerSessionMaxChannels", 0);
			_srv_free_session_expired = _cfg.get<uint32_t>("srv.FreeSessionExpired", 10);
			if (!_daemon)
				_daemon = 1 == _cfg.get<uint32_t>("srv.Daemon", 0);

			_rds_ip = _cfg.get<std::string>("redis.IP", "127.0.0.1");
			_rds_port = _cfg.get<uint16_t>("redis.Port", 6379);
			_rds_passwd = _cfg.get<std::string>("redis.Passwd", "");

			_chann_mtu = _cfg.get<uint32_t>("channel.MTU", 1500);
			_chann_port_expired = _cfg.get<uint32_t>("channel.PortExpired", 30);
			_chann_display_interval = _cfg.get<uint32_t>("channel.DisplayInterval", 0);

			loadLocalIp(_srv_ips);
			//print();
			return true;
		}
		catch (boost::property_tree::ini_parser_error &e)
		{
			std::cout << "read ini error: " << e.what() << std::endl;
			return false;
		}
	}

	/////////////////////////////////////////////////////////////////////
	std::string procName() const { return _proc_name; }
	std::string logPath() const { return _log_path; }
	size_t logRotationSize() const { return _log_rotation_size; }
	uint8_t logPrintLevel() const { return _log_print_level; }

	bool daemon() const { return _daemon; }
	std::string srvIP() const { return _srv_ip; }
	std::vector<std::string> srvIPs() const { return _srv_ips; }
	uint16_t listenPort() const { return _srv_port; }
	uint8_t IOWorkers() const { return _srv_workers; }
	uint32_t maxSessions() const { return _srv_max_sessions; }
	uint32_t maxChannels() const { return _srv_max_channels; }
	uint32_t perSessionMaxChannels() const { return _srv_per_session_max_channels; }
	uint32_t freeSessionExpired() const { return _srv_free_session_expired; }

	std::string redisIP() const { return _rds_ip; }
	uint16_t redisPort() const { return _rds_port; }
	std::string redisPasswd() const { return _rds_passwd; }

	uint32_t channMTU() const { return _chann_mtu; }
	uint32_t channPortExpired() const { return _chann_port_expired; }
	uint32_t channDisplayInterval() const { return _chann_display_interval; }

	/////////////////////////////////////////////////////////////////////
	std::string print()
	{
		std::stringstream ss;
		ss << "[config file: " << _cfg_file
			<< "][log path: " << logPath()
			<< "][log rotation size: " << logRotationSize()
			<< "][log level: " << (int)logPrintLevel()
			<< "][server ip: " << srvIP()
			<< "][listen port: " << listenPort()
			<< "][workers: " << (int)IOWorkers()
			<< "][max sessions: " << maxSessions()
			<< "][max channels: " << maxChannels()
			<< "][per session max channels: " << perSessionMaxChannels()
			<< "][free session expired: " << freeSessionExpired()
			<< "][redis addr: " << redisIP()
			<< "][redis port: " << redisPort()
			<< "][channel mtu: " << channMTU()
			<< "][channel port expired: " << channPortExpired()
			<< "][channel display interval: " << channDisplayInterval()
			<< "][is daemon: " << std::boolalpha << daemon()
			<< "]";
		return ss.str();
	}

protected:
	bool loadLocalIp(std::vector<std::string>&iplist)
	{
		iplist.clear();
		struct ifaddrs * if_addrs = NULL;
		if (getifaddrs(&if_addrs) != 0) {
			return false;
		}

		for (struct ifaddrs *pif = if_addrs; pif != NULL; pif = pif->ifa_next) {
			if (pif->ifa_addr == NULL)
				continue;

			std::string ifname = pif->ifa_name;
			if (pif->ifa_addr->sa_family == AF_INET &&
				ifname.find("lo") == std::string::npos &&
				ifname.find("tun") == std::string::npos)
			{
				void* sin_addr = &((struct sockaddr_in *)pif->ifa_addr)->sin_addr;
				char addr_buf[INET_ADDRSTRLEN] = {0};
				inet_ntop(AF_INET, sin_addr, addr_buf, INET_ADDRSTRLEN);
				iplist.push_back(addr_buf);
			}
		}
		freeifaddrs(if_addrs);
		return true;
	}

	bool parseCmd(int argc, char* argv[])
	{
		_proc_name = argv[0];
		size_t pos = _proc_name.find_last_of('/');
		_proc_name = _proc_name.substr(pos+1);

		boost::program_options::options_description desc(_proc_name + " allow option");
		desc.add_options()
				("help,h", "help message")
				("version,v", "version")
				("file,f", boost::program_options::value<std::string>(&_cfg_file), "config file path")
				("log,l", boost::program_options::value<std::string>(&_log_path), "log storage directory")
				("daemon,d", boost::program_options::value<bool>(&_daemon), "daemon option");

		boost::program_options::variables_map vm;
		boost::program_options::store(
				boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

		if (vm.count("help")) {
			std::cout << "help: " << desc << std::endl;
			return false;
		}

		if (vm.count("version")) {
			std::cout << "version: " << BUILD_VERSION
#if !defined(NDEBUG)
			<< "(Debug)"
#else
			<< "(Release)"
#endif
					<< std::endl;
			return false;
		}

		if (!vm.count("file"))
			_cfg_file += _proc_name + ".cfg";

		return true;
	}

	CConfig()
	: _proc_name("")
	, _cfg_file("/usr/local/etc/")
	, _log_path("/usr/local/log/")
	, _log_rotation_size(0)
	, _log_print_level(0)
	, _daemon(false)
	, _srv_ips()
	, _srv_ip("")
	, _srv_port(0)
	, _srv_workers(0)
	, _srv_max_sessions(0)
	, _srv_max_channels(0)
	, _srv_per_session_max_channels(0)
	, _srv_free_session_expired(0)
	, _rds_ip("")
	, _rds_port(0)
	, _chann_mtu(1500)
	, _chann_port_expired(0)
	, _chann_display_interval(0)
	{}

private:
	boost::property_tree::ptree _cfg;

	std::string _proc_name; // 进程名称
	std::string _cfg_file; // 配置文件 (绝对路径)

	std::string _log_path; // 日志路径
	size_t  	_log_rotation_size;// 日志旋转大小(MB)
	uint8_t 	_log_print_level; // 日志输出等级

	bool		_daemon;
	std::vector<std::string> _srv_ips; // 所有网卡IP
	std::string _srv_ip; // 指定绑定IP
	uint16_t 	_srv_port; // 监听端口
	uint8_t		_srv_workers; // IO线程数
	uint32_t 	_srv_max_sessions; // 最大用户数 (0 未限制)
	uint32_t 	_srv_max_channels; // 最大通道数 (0 未限制)
	uint32_t	_srv_per_session_max_channels; // 每用户最大通道数 (0 未限制)
	uint32_t 	_srv_free_session_expired; // 未登陆用户过期时间(秒)

	std::string _rds_ip; // redis ip
	uint16_t 	_rds_port; // redis port
	std::string _rds_passwd; // redis 密码

	uint32_t	_chann_mtu; // 通道MTU
	uint32_t	_chann_port_expired; // 通道端口过期时间(秒)
	uint32_t 	_chann_display_interval; // 通道信息输出间隔(秒)
};

#define gConfig (CConfig::getInstance())

#endif
