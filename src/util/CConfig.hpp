﻿/*
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

			_log_file_size = _cfg.get<uintmax_t>("log.file_size", 20);
			_log_file_size = std::min<uintmax_t>(_log_file_size, 100);
			_log_file_size *= 1024 * 1024;
			_log_level = _cfg.get<uint32_t>("log.log_level", 0);
			_srv_addr = _cfg.get<std::string>("srv.addr", "0.0.0.0");
			_listen_port = _cfg.get<uint32_t>("srv.listen_port", 10001);
			_worker_num = _cfg.get<uint32_t>("srv.worker_num", 2);
			return true;
		}
		catch (boost::property_tree::ini_parser_error &e)
		{
			std::cerr << "read ini error: " << e.what() << std::endl;
			return false;
		}
	}

	static std::string getExeFullPath()
	{
		std::string fullpath = boost::filesystem::initial_path<boost::filesystem::path>().string();
		return fullpath;
	}

	/////////////////////////////////////////////////////////////////////
	std::string logPath() const { return _log_path; }
	uintmax_t logFileSize() const { return _log_file_size; }
	uint32_t logLevel() const { return _log_level; }
	std::string srvAddr() const { return _srv_addr; }
	uint32_t listenPort() const { return _listen_port; }
	uint32_t workerNum() const { return _worker_num; }
	bool daemon() const { return _daemon; }

	/////////////////////////////////////////////////////////////////////
	void print()
	{
		std::cout << "================ config ================" << std::endl;
		std::cout << "config file: " << logPath() << std::endl;
		std::cout << "log path: " << logPath() << std::endl;
		std::cout << "log file size: " << logFileSize() << std::endl;
		std::cout << "log level: " << logLevel() << std::endl;
		std::cout << "server addr: " << srvAddr() << std::endl;
		std::cout << "listen port: " << listenPort() << std::endl;
		std::cout << "worker num: " << workerNum() << std::endl;
		std::cout << "is daemon: " << std::boolalpha << daemon() << std::endl;
		std::cout << "========================================" << std::endl;
	}

protected:
	bool parseCmd(int argc, char* argv[])
	{
		std::string program(argv[0]);
		boost::program_options::options_description desc("nat proxy allow option");
		desc.add_options()
				("help,h", "help message")
				("file,f", boost::program_options::value<std::string>(&_cfg_file), "config file path")
				("log,l", boost::program_options::value<std::string>(&_log_path), "log storage directory")
				("daemon,d", boost::program_options::value<bool>(&_daemon), "daemon option");

		boost::program_options::variables_map vm;
		boost::program_options::store(
				boost::program_options::parse_command_line(argc, argv, desc), vm);
		boost::program_options::notify(vm);

		if (vm.count("help")) {
			std::cerr << "help: " << desc << std::endl;
			return false;
		}
		return true;
	}

	CConfig()
	: _cfg_file("")
	, _log_path("")
	, _log_file_size(0)
	, _log_level(0)
	, _srv_addr("")
	, _listen_port(0)
	, _worker_num(0)
	, _daemon(false)
	{}

private:
	boost::property_tree::ptree _cfg;

	std::string _cfg_file;
	std::string _log_path;
	uintmax_t 	_log_file_size;//M
	uint32_t 	_log_level;
	std::string _srv_addr;
	uint32_t 	_listen_port;
	uint32_t	_worker_num;
	bool		_daemon;
};

#define gConfig (CConfig::getInstance())

#endif
