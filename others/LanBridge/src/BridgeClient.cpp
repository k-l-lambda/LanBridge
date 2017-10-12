
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>

#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#pragma warning(push, 1)
#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#pragma warning(pop)

#include <windows.h>

#include "..\include\Common.h"
#include "..\include\Bridge.h"
#include "..\include\Log.h"

#include "..\FileSystemBridge\FileSystemBridgePitcher.h"
#include "..\FileSystemBridge\FileSystemBridgeCatcher.h"
#pragma comment(lib, "FileSystemBridge.lib")

#include "..\UdpBridge\UdpBridgePitcher.h"
#include "..\UdpBridge\UdpBridgeCatcher.h"
#pragma comment(lib, "UdpBridge.lib")

#include "..\VideoBridge\VideoBridgePitcher.h"
#include "..\VideoBridge\VideoBridgeCatcher.h"
#pragma comment(lib, "VideoBridge.lib")

#include "..\MemoryBridge\MemoryBridgePitcher.h"
#include "..\MemoryBridge\MemoryBridgeCatcher.h"
#pragma comment(lib, "MemoryBridge.lib")

#include "..\TcpBridge\TcpServer.h"
#include "..\TcpBridge\TcpBridgeCatcher.h"
#include "..\TcpBridge\TcpBridgePitcher.h"
#pragma comment(lib, "TcpBridge.lib")


boost::asio::io_service io_service;

namespace LanBridgeClient
{
	static boost::mutex s_LogMutex;

	std::string g_RequestsDir, g_ReponsesDir;

	boost::scoped_ptr<Bridge>	g_Bridge;

	bool		g_LogRequests = false;
	bool		g_LogResponse = false;
	bool		g_LogBySession = false;


	std::string getComputerName()
	{
		char buffer[0x100];
		::DWORD size = 0x100;
		::GetComputerName(buffer, &size);

		return buffer;
	}

	std::string getUserName()
	{
		char buffer[0x100];
		::DWORD size = 0x100;
		::GetUserName(buffer, &size);

		return buffer;
	}

	static std::string s_LocalId = getComputerName() + "_" + getUserName();

	std::string genConnectionId()
	{
		std::srand(std::clock());
		static size_t index = 0;

		return s_LocalId + "_" + boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time()) + "_" + boost::lexical_cast<std::string>(std::rand() + (index++));
	}

	std::string now()
	{
		return boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time());
	}


	const unsigned long	s_DefaultInterval = 100;

	static std::ofstream s_RequestsLog;
	static std::ofstream s_ResponsesLog;

	struct SessionLog
	{
		std::map<std::string, boost::shared_ptr<std::ofstream> >	m_StreamMap;

		std::ofstream&	get(const std::string& id)
		{
			if(!m_StreamMap.count(id))
				m_StreamMap.insert(std::make_pair(id, boost::shared_ptr<std::ofstream>(new std::ofstream(("logs\\" + id + ".log").data(), std::ios::app))));

			return *(m_StreamMap[id]);
		};

		void	close(const std::string& id)
		{
			if(m_StreamMap.count(id))
				m_StreamMap.erase(id);
		};
	};

	static SessionLog& sessionLog()
	{
		static SessionLog log;
		return log;
	};


	void session_input(socket_ptr sock, const std::string& connection_id)
	{
		try
		{
			while(sock->is_open())
			{
				//const std::string response_filename = g_ReponsesDir + connection_id + ".response";
				{
					char response_buffer[s_PackLength];
					const size_t length = g_Bridge->read(connection_id, response_buffer);

					if(length)
					{
						boost::asio::write(*sock, boost::asio::buffer(response_buffer, length));

						if(g_LogResponse)
						{
							boost::mutex::scoped_lock lock(s_LogMutex);

							s_ResponsesLog << std::endl << "[" << now() << "]\t" << "response " << connection_id << ":" << std::endl;
							s_ResponsesLog.write(response_buffer, length);
							s_ResponsesLog << std::endl;
						}

						if(g_LogBySession)
						{
							boost::mutex::scoped_lock lock(s_LogMutex);

							std::ofstream& log = sessionLog().get(connection_id);
							log << ">>>>>>\t" << now() << std::endl;
							log.write(response_buffer, length);
							log << std::endl << std::endl;
						}
					}
					else
					{
						Log::shell(Log::Msg_Input) << "[" << connection_id << "]	response is empty, session end.";

						if(g_LogBySession)
						{
							boost::mutex::scoped_lock lock(s_LogMutex);

							sessionLog().get(connection_id) << ">>\t" << now() << "\tEND" << std::endl;
							sessionLog().close(connection_id);
						}

						break;
					}

					Log::shell(Log::Msg_Input) << "[" << connection_id << "]	response: " << length << " bytes.";
				}
			}
		}
		catch(const std::exception& e)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	input exception: " << e.what();
		}

		if(sock->is_open())
			sock->close();
	}


	void session(socket_ptr sock)
	{
		assert(!g_LogRequests || s_RequestsLog.is_open());
		assert(!g_LogResponse || s_ResponsesLog.is_open());

		const std::string connection_id = genConnectionId();

		try
		{
			const std::string filename = g_RequestsDir + connection_id;

			boost::thread inputthread(boost::bind(&session_input, sock, connection_id));

			while(sock->is_open())
			{
				char data[s_PackLength];
				boost::system::error_code error;
				size_t length = sock->read_some(boost::asio::buffer(data, s_PackLength), error);
				if(error == boost::asio::error::eof)
				{
					Log::shell(Log::Msg_Clew) << "[" << connection_id << "]	connection closed by peer.";

					if(g_LogBySession)
					{
						boost::mutex::scoped_lock lock(s_LogMutex);

						sessionLog().get(connection_id) << "<<\t" << now() << "\tPEER END" << std::endl;
						sessionLog().close(connection_id);
					}

					if(inputthread.timed_join(boost::posix_time::seconds(0)))
						break;
				}
				else if(error)
					throw boost::system::system_error(error); // Some other error.

				g_Bridge->write(connection_id, data, length);

				{
					char* end = std::find(data, data + length, '\n');
					Log::shell(Log::Msg_Output) << "[" << connection_id << "]	request sent: " << length << " bytes: " << std::string(data, end - data);
				}

				if(g_LogRequests)
				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					s_RequestsLog << std::endl << "[" << now() << "]\t" << "request " << connection_id << ":" << std::endl;
					s_RequestsLog.write(data, length);
					s_RequestsLog << std::endl << std::endl;
				}

				if(g_LogBySession)
				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::ofstream& log = sessionLog().get(connection_id);
					log << "<<<<<<\t" << now() << std::endl;
					log.write(data, length);
					log << std::endl;
				}
			}

			g_Bridge->write(connection_id, NULL, 0);

			if(sock->is_open())
				sock->close();
		}
		catch(const std::exception& e)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	exception: " << e.what();

			if(sock->is_open())
				sock->close();
		}
	}


	int main(int argc, char* argv[])
	{
		namespace po = boost::program_options;

		po::options_description desc("Allowed options");
		desc.add_options()
			("port",						po::value<short>(),			"port")
			("localid",						po::value<std::string>(),	"local user id")
			("interval",					po::value<unsigned long>()->implicit_value(400),	"directory lookup interval")
			("log_requests",				po::value<bool>()->implicit_value(true))
			("log_response",				po::value<bool>()->implicit_value(true))
			("log_by_session",				po::value<bool>()->implicit_value(true))
			("pitcher",						po::value<std::string>())
			("catcher",						po::value<std::string>())
			("station",						po::value<std::string>(),	"data transfer station")
			("udp_pitcher_host",			po::value<std::string>())
			("udp_pitcher_port",			po::value<std::string>())
			("udp_pitcher_interval",		po::value<unsigned long>())
			("udp_catcher_port",			po::value<unsigned short>())
			("video_pitcher_videoformat",	po::value<int>())
			("video_catcher_videoformat",	po::value<int>())
			("video_catcher_interval",		po::value<unsigned long>())
			("memory_pitcher_repository",	po::value<std::string>())
			("memory_catcher_repository",	po::value<std::string>())
			("tcp_server_port",				po::value<unsigned short>())
			("tcp_server_password",			po::value<std::string>()->default_value("LanBridgeTcpBridge"))
		;

		try
		{
			po::variables_map vm;
			po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
			po::notify(vm);

			const short port = vm["port"].as<short>();
			const std::string station = vm.count("station") ? vm["station"].as<std::string>() : "";
			const unsigned long interval = vm.count("interval") ? vm["interval"].as<unsigned long>() : s_DefaultInterval;
			g_LogRequests = vm.count("log_requests") && vm["log_requests"].as<bool>();
			g_LogResponse = vm.count("log_response") && vm["log_response"].as<bool>();
			g_LogBySession = vm.count("log_by_session") && vm["log_by_session"].as<bool>();

			if(g_LogRequests)
				s_RequestsLog.open("requests.log", std::ios::app);
			if(g_LogResponse)
				s_ResponsesLog.open("responses.log", std::ios::app);

			if(vm.count("tcp_server_port"))
			{
				const unsigned short port = vm["tcp_server_port"].as<unsigned short>();
				const std::string password = vm["tcp_server_password"].as<std::string>();

				boost::shared_ptr<TcpBridge::TcpServer>& server = TcpBridge::TcpServer::instance();
				server.reset(new TcpBridge::TcpServer(port, password));
			}

			const std::string pitchertype = vm.count("pitcher") ? vm["pitcher"].as<std::string>() : "FileSystem";
			const std::string catchertype = vm.count("catcher") ? vm["catcher"].as<std::string>() : "FileSystem";

			PitcherPtr pitcher;
			if(pitchertype == "FileSystem")
				pitcher.reset(new FileSystemBridge::Pitcher(station + "\\requests\\"));
			else if(pitchertype == "UDP")
			{
				const std::string udp_pitcher_host = vm["udp_pitcher_host"].as<std::string>();
				const std::string udp_pitcher_port = vm["udp_pitcher_port"].as<std::string>();
				const unsigned long udp_pitcher_interval = vm.count("udp_pitcher_interval") ? vm["udp_pitcher_interval"].as<unsigned long>() : 10;

				pitcher.reset(new UdpBridge::Pitcher(io_service, udp_pitcher_host, udp_pitcher_port, udp_pitcher_interval));
			}
			else if(pitchertype == "Memory")
			{
				pitcher.reset(new MemoryBridge::Pitcher(vm["memory_pitcher_repository"].as<std::string>()));
			}
			else if(pitchertype == "TCP")
			{
				if(!TcpBridge::TcpServer::instance())
					throw std::runtime_error("TcpServer missing, pitcher create failed.");

				pitcher.reset(new TcpBridge::Pitcher(TcpBridge::TcpServer::instance()->getSocket(), interval));
			}
			else
				throw std::runtime_error("unknown pitcher: " + pitchertype);

			CatcherPtr catcher;
			if(catchertype == "FileSystem")
				catcher.reset(new FileSystemBridge::Catcher(s_PackLength, station + "\\responses\\", interval));
			else if(catchertype == "UDP")
			{
				const unsigned short udp_catcher_port = vm["udp_catcher_port"].as<unsigned short>();

				catcher.reset(new UdpBridge::Catcher(s_PackLength, io_service, udp_catcher_port, interval));
			}
			else if(catchertype == "Video")
			{
				const int video_catcher_videoformat = vm.count("video_catcher_videoformat") ? vm["video_catcher_videoformat"].as<int>() : VideoBridge::VF_24bits;
				const unsigned long video_catcher_interval = vm.count("video_catcher_interval") ? vm["video_catcher_interval"].as<unsigned long>() : 160;

				catcher.reset(new VideoBridge::Catcher(s_PackLength, video_catcher_interval, VideoBridge::VideoFormat(video_catcher_videoformat)));
			}
			else if(catchertype == "Memory")
			{
				catcher.reset(new MemoryBridge::Catcher(s_PackLength, interval, vm["memory_catcher_repository"].as<std::string>()));
			}
			else if(catchertype == "TCP")
			{
				if(!TcpBridge::TcpServer::instance())
					throw std::runtime_error("TcpServer missing, catcher create failed.");

				catcher.reset(new TcpBridge::Catcher(TcpBridge::TcpServer::instance()->getSocket(), s_PackLength, interval));
			}
			else
				throw std::runtime_error("unknown catcher: " + catchertype);

			g_Bridge.reset(new Bridge(pitcher, catcher));

			if(vm.count("localid"))
				s_LocalId = vm["localid"].as<std::string>();

			{
				Log::shell(Log::Msg_SetUp) << "server started.";

				tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
				for(;;)
				{
					socket_ptr sock(new tcp::socket(io_service));
					a.accept(*sock);
					boost::thread t(boost::bind(session, sock));
				}
			}
		}
		catch(const std::exception& e)
		{
			Log::shell(Log::Msg_Fatal) << "Exception: " << e.what();
		}

		std::system("pause");

		return 0;
	}
}
