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
#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>
#include "version.h"

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

			_log_file_size = _cfg.get<size_t>("log.file_size", 20);
			_log_file_size = std::min<size_t>(_log_file_size, 100);
			_log_level = _cfg.get<uint8_t>("log.log_level", 0);
			_srv_addr = _cfg.get<std::string>("srv.addr", "0.0.0.0");
			_listen_port = _cfg.get<uint16_t>("srv.listen_port", 10001);
			_worker_num = _cfg.get<uint8_t>("srv.worker_num", 2);
			_login_timeout = _cfg.get<uint32_t>("srv.login_timeout", 30);
			if (!_daemon)
				_daemon = 1 == _cfg.get<uint32_t>("srv.daemon", 0);

			_chann_rbuff_size = _cfg.get<uint32_t>("channel.rbuff_size", 1500);
			_chann_sbuff_size = _cfg.get<uint32_t>("channel.sbuff_size", 1500);
			_chann_display_timeout = _cfg.get<uint32_t>("channel.display_timeout", 60);
			if (_chann_display_timeout == 0)
				_chann_display_timeout = 60;

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
	size_t logFileSize() const { return _log_file_size; }
	uint8_t logLevel() const { return _log_level; }
	std::string srvAddr() const { return _srv_addr; }
	uint16_t listenPort() const { return _listen_port; }
	uint8_t workerNum() const { return _worker_num; }
	uint32_t loginTimeout() const { return _login_timeout; }
	uint32_t channReceiveBuffSize() const { return _chann_rbuff_size; }
	uint32_t channSendBuffSize() const { return _chann_sbuff_size; }
	uint32_t channDisplayTimeout() const { return _chann_display_timeout; }
	bool daemon() const { return _daemon; }

	/////////////////////////////////////////////////////////////////////
	std::string print()
	{
		std::stringstream ss;
		ss << "[config file: " << _cfg_file
			<< "][log path: " << logPath()
			<< "][log file size: " << logFileSize()
			<< "][log level: " << (int)logLevel()
			<< "][server addr: " << srvAddr()
			<< "][listen port: " << listenPort()
			<< "][worker num: " << (int)workerNum()
			<< "][login timeout: " << loginTimeout()
			<< "][channel receive buffer size: " << channReceiveBuffSize()
			<< "][channel send buffer size: " << channSendBuffSize()
			<< "][channel display timeout: " << channDisplayTimeout()
			<< "][is daemon: " << std::boolalpha << daemon()
			<< "]";
		return ss.str();
	}

protected:
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
			std::cout << "version: " << BUILD_VERSION << std::endl;
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
	, _log_file_size(0)
	, _log_level(0)
	, _srv_addr("")
	, _listen_port(0)
	, _worker_num(0)
	, _login_timeout(0)
	, _chann_rbuff_size(1500)
	, _chann_sbuff_size(1500)
	, _chann_display_timeout(0)
	, _daemon(false)
	{}

private:
	boost::property_tree::ptree _cfg;

	std::string _proc_name;
	std::string _cfg_file;
	std::string _log_path;
	size_t  	_log_file_size;//M
	uint8_t 	_log_level;
	std::string _srv_addr;
	uint16_t 	_listen_port;
	uint8_t		_worker_num;
	uint32_t 	_login_timeout;
	uint32_t 	_chann_rbuff_size;
	uint32_t	_chann_sbuff_size;
	uint32_t 	_chann_display_timeout;
	bool		_daemon;
};

#define gConfig (CConfig::getInstance())

#endif
