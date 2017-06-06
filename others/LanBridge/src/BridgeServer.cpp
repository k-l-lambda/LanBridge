
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>

#pragma warning(push, 1)
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>
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

#include "..\TcpBridge\TcpClient.h"
#include "..\TcpBridge\TcpBridgeCatcher.h"
#include "..\TcpBridge\TcpBridgePitcher.h"
#pragma comment(lib, "TcpBridge.lib")


namespace LanBridgeServer
{
	using boost::asio::ip::tcp;

	static const size_t max_length = 0x2000;

	std::string g_RequestsDir, g_ReponsesDir;

	static boost::mutex s_LogMutex;
	static boost::mutex s_HostMapMutex;

	boost::scoped_ptr<Bridge>	g_Bridge;

	std::map<std::string, std::string>		g_HostMap;


	std::string trimString(const std::string& s)
	{
		std::string r = s;
		while(!r.empty() && r[r.size() - 1] == '\0')
			r = r.substr(0, r.size() - 1);

		return r;
	}

	void loadHostMap()
	{
		std::ifstream file("hosts");
		if(file.is_open())
		{
			g_HostMap.clear();

			while(!file.eof())
			{
				char line[0x100];
				file.getline(line, 0x100);

				const std::string sline(line, 0x100);

				boost::smatch what;
				bool result = boost::regex_match(sline, what, boost::regex("(\\S+)\\s+(\\S+).*"));
				if(result && what.size() >= 3)
				{
					g_HostMap.insert(std::make_pair(what[2].str().c_str(), what[1].str().c_str()));
				}
			}

			Log::shell() << "host map loaded.";
		}
	}


	std::string parseCommand(const std::string& request)
	{
		boost::smatch what;
		bool result = boost::regex_match(request, what, boost::regex("(\\w+)\\s.*"));
		if(result && what.size() >= 2)
		{
			return what[1];
		}

		return "";
	}

	void doParseHost(const std::string& request, std::string& host, std::string& port)
	{
		boost::smatch what;
		bool result = boost::regex_match(request, what, boost::regex("(\\w+)\\s+(\\S+)(\\s+.*)?"));
		if(result && what.size() >= 3)
		{
			const std::string command = what[1].str();
			const std::string path = what[2].str();

			if(command == "CONNECT")
			{
				boost::smatch what2;
				bool result2 = boost::regex_match(path, what2, boost::regex("([^/]+?)\\:(\\d+)"));
				if(result2)
				{
					assert(what2.size() >= 3);

					host = what2[1].str();
					port = what2[2].str();

					return;
				}
				else
					throw std::runtime_error("request path parse failed: " + path);
			}
			else
			{
				boost::smatch what2;
				bool result2 = boost::regex_match(path, what2, boost::regex("\\w+\\:/*([^/]+?)\\:(\\d+)(/.*)?"));
				if(result2)
				{
					assert(what2.size() >= 3);

					host = what2[1].str();
					port = what2[2].str();

					return;
				}
				else
				{
					boost::smatch what3;
					bool result3 = boost::regex_match(path, what3, boost::regex("\\w+\\:/*([^/]+)(/.*)?"));
					if(result3)
					{
						assert(what3.size() >= 2);

						host = what3[1].str();
						port = "80";

						return;
					}
					else
						throw std::runtime_error("request path parse failed: " + path);
				}
			}
		}
		else
			throw std::runtime_error("request parse failed: " + std::string(request));
	}

	void parseHost(const std::string& request, std::string& host, std::string& port)
	{
		doParseHost(request, host, port);

		{
			boost::mutex::scoped_lock lock(s_HostMapMutex);

			if(g_HostMap.count(host))
				host = g_HostMap[host];
		}
	}

	std::string	translateHeaders(const std::string& request)
	{
		// remove host name from URL in start line
		std::string result = boost::regex_replace(request, boost::regex("^([A-Z]+\\s+)(\\w+:/*[^/]+)(\\S+\\s+[A-Z]+/[\\d\\.]+)$"), "\\1\\3");

		// convert header: "Proxy-Connection" -> "Connection"
		result = boost::regex_replace(result, boost::regex("^(Proxy-Connection:)", boost::regex_constants::icase), "Connection:");
		//result = boost::regex_replace(result, boost::regex("^Connection:.*$", boost::regex_constants::icase), "");

		return result;
	}


	void session_output(socket_ptr sock, const std::string& connection_id)
	{
		try
		{
			unsigned long interval = 500;
			while(sock->is_open())
			{
				char reply[max_length];
				boost::system::error_code error;
				size_t reply_length = sock->read_some(boost::asio::buffer(reply, max_length), error);
				if(error == boost::asio::error::eof || reply_length == 0)
				{
					{
						if(error == boost::asio::error::eof)
							Log::shell(Log::Msg_Clew) << "[" << connection_id << "]	EOF.";
						else if(reply_length == 0)
							Log::shell(Log::Msg_Clew) << "[" << connection_id << "]	0 byte replied.";
					}

					if(interval < 2000)
						interval *= 2;
					else
					{
						// end session
						g_Bridge->write(connection_id, reply, 0);

						break;
					}
					::Sleep(interval);

					continue;
				}

				Log::shell(Log::Msg_Input) << "[" << connection_id << "]	reply: " << reply_length << " bytes received.";

				g_Bridge->write(connection_id, reply, reply_length);
			}
		}
		catch(const std::exception& e)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	exception: " << e.what();

			try
			{
				if(sock->is_open())
					sock->close();
			}
			catch(...)
			{
			}
		}
		catch(...)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	unknown exception.";
		}
	}

	void session(boost::asio::io_service& io_service, const std::string& connection_id)
	{
		try
		{
			socket_ptr sock(new tcp::socket(io_service));

			boost::scoped_ptr<boost::thread> outputthread;

			const std::string filename = g_RequestsDir + connection_id + ".data";

			for(;;)
			{
				char request_buffer[s_PackLength];
				const size_t length = g_Bridge->read(connection_id, request_buffer);

				//if(request_buffer.empty())
				if(!length)
				{
					Log::shell(Log::Msg_Remove) << "[" << connection_id << "]	0 byte request received, session end.";

					//throw std::runtime_error("request buffer is empty.");
					break;
				}

				const std::string command = parseCommand(request_buffer);
				if(command.empty() && !sock->is_open())
				{
					Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	error request header, session end.";

					break;
				}

				const bool firstPack = !sock->is_open();

				if(!sock->is_open())
				{
					std::string host, port;
					parseHost(request_buffer, host, port);

					tcp::resolver resolver(io_service);
					boost::scoped_ptr<tcp::resolver::query> query;
					tcp::resolver::iterator iterator;

					try
					{
						query.reset(new tcp::resolver::query(tcp::v4(), host, port));
						iterator = resolver.resolve(*query);
					}
					catch(const boost::system::system_error& e)
					{
						if(e.code().value() == boost::asio::error::host_not_found)
						{
							// try again with ipv6
							query.reset(new tcp::resolver::query(tcp::v6(), host, port));
							iterator = resolver.resolve(*query);
						}
						else
							throw e;
					}

					for(; iterator != tcp::resolver::iterator(); ++iterator)
					{
						try
						{
							sock->connect(*iterator);
							assert(sock->is_open());

							Log::shell(Log::Msg_SetUp) << "[" << connection_id << "]	connection of \"" << host << ":" << port << "\" setup.";

							break;
						}
						catch(const std::exception& e)
						{
							Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	request connect failed: " << e.what();
						}
					}
				}

				if(command == "CONNECT")
				{
					//static const std::string reply = "HTTP/1.1 100 Continue\r\n\r\n";
					static const std::string reply = "HTTP/1.1 200 OK\r\n\r\n";
					//static const std::string reply = "HTTP/1.1 204 No Content\r\n\r\n";

					g_Bridge->write(connection_id, reply.data(), reply.length());
				}
				else
				{
					const char* sent_buffer = request_buffer;
					size_t sent_length = length;
					std::string translation;
					if(firstPack)
					{
						translation = translateHeaders(std::string(request_buffer, length));
						sent_buffer = translation.data();
						sent_length = translation.length();
					}

					boost::asio::write(*sock, boost::asio::buffer(sent_buffer, sent_length));
				}

				if(!outputthread)
					outputthread.reset(new boost::thread(boost::bind(&session_output, boost::ref(sock), connection_id)));
			}

			if(sock->is_open())
				sock->close();
		}
		catch(const std::exception& e)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	Exception: " << e.what();
		}
		catch(...)
		{
			Log::shell(Log::Msg_Warning) << "[" << connection_id << "]	unknown exception.";
		}
	}


	static boost::asio::io_service io_service;


	int main(int argc, char* argv[])
	{
		namespace po = boost::program_options;

		po::options_description desc("Allowed options");
		desc.add_options()
			("interval",	po::value<unsigned long>()->implicit_value(400),	"directory lookup interval")
			("usage",		po::value<std::string>())
			("pitcher",		po::value<std::string>())
			("catcher",		po::value<std::string>())
			("station",		po::value<std::string>(),							"data transfer station")
			("udp_pitcher_host",		po::value<std::string>())
			("udp_pitcher_port",		po::value<std::string>())
			("udp_pitcher_interval",	po::value<unsigned long>())
			("udp_catcher_port",		po::value<unsigned short>())
			("video_pitcher_framex",	po::value<size_t>())
			("video_pitcher_framey",	po::value<size_t>())
			("video_pitcher_framewidth",	po::value<size_t>())
			("video_pitcher_frameheight",	po::value<size_t>())
			("video_pitcher_videoformat",	po::value<int>())
			("video_catcher_videoformat",	po::value<int>())
			("memory_pitcher_repository",	po::value<std::string>())
			("memory_catcher_repository",	po::value<std::string>())
			("tcp_client_host",				po::value<std::string>())
			("tcp_client_port",				po::value<std::string>())
			("tcp_client_password",			po::value<std::string>()->default_value("LanBridgeTcpBridge"))
		;

		try
		{
			po::variables_map vm;
			po::store(po::parse_command_line(argc, argv, desc), vm);
			po::notify(vm);

			unsigned long interval = vm.count("interval") ? vm["interval"].as<unsigned long>() : 400;

			if(vm.count("tcp_client_host"))
			{
				const std::string host = vm["tcp_client_host"].as<std::string>();
				const std::string port = vm["tcp_client_port"].as<std::string>();
				const std::string password = vm["tcp_client_password"].as<std::string>();

				boost::shared_ptr<TcpBridge::TcpClient>& client = TcpBridge::TcpClient::instance();
				client.reset(new TcpBridge::TcpClient(host, port, password));
			}

			const std::string pitchertype = vm.count("pitcher") ? vm["pitcher"].as<std::string>() : "FileSystem";
			const std::string catchertype = vm.count("catcher") ? vm["catcher"].as<std::string>() : "FileSystem";

			PitcherPtr pitcher;
			if(pitchertype == "FileSystem")
			{
				const std::string station = vm["station"].as<std::string>();

				pitcher.reset(new FileSystemBridge::Pitcher(station + "\\responses\\"));
			}
			else if(pitchertype == "UDP")
			{
				const std::string udp_pitcher_host = vm["udp_pitcher_host"].as<std::string>();
				const std::string udp_pitcher_port = vm["udp_pitcher_port"].as<std::string>();
				const unsigned long udp_pitcher_interval = vm.count("udp_pitcher_interval") ? vm["udp_pitcher_interval"].as<unsigned long>() : 10;

				pitcher.reset(new UdpBridge::Pitcher(io_service, udp_pitcher_host, udp_pitcher_port, udp_pitcher_interval));
			}
			else if(pitchertype == "Video")
			{
				const size_t video_pitcher_framex = vm.count("video_pitcher_framex") ? vm["video_pitcher_framex"].as<size_t>() : 0;
				const size_t video_pitcher_framey = vm.count("video_pitcher_framey") ? vm["video_pitcher_framey"].as<size_t>() : 0;
				const size_t video_pitcher_framewidth = vm.count("video_pitcher_framewidth") ? vm["video_pitcher_framewidth"].as<size_t>() : 128;
				const size_t video_pitcher_frameheight = vm.count("video_pitcher_frameheight") ? vm["video_pitcher_frameheight"].as<size_t>() : 128;
				const int video_pitcher_videoformat = vm.count("video_pitcher_videoformat") ? vm["video_pitcher_videoformat"].as<int>() : VideoBridge::VF_24bits;

				pitcher.reset(new VideoBridge::Pitcher(video_pitcher_framewidth, video_pitcher_frameheight, VideoBridge::VideoFormat(video_pitcher_videoformat), video_pitcher_framex, video_pitcher_framey));
			}
			else if(pitchertype == "Memory")
			{
				pitcher.reset(new MemoryBridge::Pitcher(vm["memory_pitcher_repository"].as<std::string>()));
			}
			else if(pitchertype == "TcpClient")
			{
				// TODO:
			}
			else
				throw std::runtime_error("unknown pitcher: " + pitchertype);

			CatcherPtr catcher;
			if(catchertype == "FileSystem")
			{
				const std::string station = vm["station"].as<std::string>();

				catcher.reset(new FileSystemBridge::Catcher(s_PackLength, station + "\\requests\\", interval));
			}
			else if(catchertype == "UDP")
			{
				const unsigned short udp_catcher_port = vm["udp_catcher_port"].as<unsigned short>();

				catcher.reset(new UdpBridge::Catcher(s_PackLength, io_service, udp_catcher_port, interval));
			}
			else if(catchertype == "Memory")
			{
				catcher.reset(new MemoryBridge::Catcher(s_PackLength, interval, vm["memory_catcher_repository"].as<std::string>()));
			}
			else if(catchertype == "TcpClient")
			{
				catcher.reset(new TcpBridge::Catcher(TcpBridge::TcpClient::instance()->getSocket(), s_PackLength, interval));
			}
			else
				throw std::runtime_error("unknown catcher: " + catchertype);


			loadHostMap();

			g_Bridge.reset(new Bridge(pitcher, catcher, interval));
			if(catcher)
				g_Bridge->acceptConnections(boost::bind(session, boost::ref(io_service), _1));
		}
		catch (std::exception& e)
		{
			Log::shell(Log::Msg_Fatal) << "Exception: " << e.what();
		}
		catch(...)
		{
			Log::shell(Log::Msg_Fatal) << "Unknown exception.";
		}

		std::system("pause");

		return 0;
	}
}
