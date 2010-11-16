
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>

#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include <windows.h>

#include "..\include\Common.h"
#include "..\include\Bridge.h"

#include "..\FileSystemBridge\FileSystemBridgePitcher.h"
#include "..\FileSystemBridge\FileSystemBridgeCatcher.h"
#pragma comment(lib, "FileSystemBridge.lib")

#include "..\UdpBridge\UdpBridgePitcher.h"
#include "..\UdpBridge\UdpBridgeCatcher.h"
#pragma comment(lib, "UdpBridge.lib")

#include "..\VideoBridge\VideoBridgePitcher.h"
#include "..\VideoBridge\VideoBridgeCatcher.h"
#pragma comment(lib, "VideoBridge.lib")


static boost::mutex s_LogMutex;

std::string g_RequestsDir, g_ReponsesDir;

boost::scoped_ptr<Bridge>	g_Bridge;


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


const unsigned long	s_DefaultInterval = 400;

static std::ofstream s_RequestsLog("requests.log", std::ios::app);
static std::ofstream s_ResponsesLog("responses.log", std::ios::app);


void session_input(socket_ptr sock, const std::string& connection_id)
{
	try
	{
		while(sock->is_open())
		{
			const std::string response_filename = g_ReponsesDir + connection_id + ".response";
			{
				char response_buffer[s_PackLength];
				const size_t length = g_Bridge->read(connection_id, response_buffer);

				if(length)
				{
					boost::asio::write(*sock, boost::asio::buffer(response_buffer, length));

					{
						boost::mutex::scoped_lock lock(s_LogMutex);

						s_ResponsesLog << std::endl << "[" << now() << "]\t" << "response " << connection_id << ":" << std::endl;
						s_ResponsesLog.write(response_buffer, length);
						s_ResponsesLog << std::endl;
					}
				}
				else
				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::cerr << "->	[" << connection_id << "]	response is empty, session end." << std::endl;

					break;
				}

				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::cout << "->	[" << connection_id << "]	response: " << length << " bytes." << std::endl;
				}
			}
		}
	}
	catch(const std::exception& e)
	{
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "*	[" << connection_id << "]	input exception: " << e.what() << std::endl;
	}

	if(sock->is_open())
		sock->close();
}


void session(socket_ptr sock)
{
	assert(s_RequestsLog.is_open());
	assert(s_ResponsesLog.is_open());

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
				break; // Connection closed cleanly by peer.
				//return; // Connection closed cleanly by peer.
			else if(error)
				throw boost::system::system_error(error); // Some other error.

			//writeRequest(connection_id, data, length);
			g_Bridge->write(connection_id, data, length);

			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				char* end = std::find(data, data + length, '\n');
				std::cout << "<-	[" << connection_id << "]	request sent: " << length << " bytes: " << std::string(data, end - data) << std::endl;
			}

			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				s_RequestsLog << std::endl << "[" << now() << "]\t" << "request " << connection_id << ":" << std::endl;
				s_RequestsLog.write(data, length);
				s_RequestsLog << std::endl;
			}
		}

		g_Bridge->write(connection_id, NULL, 0);

		if(sock->is_open())
			sock->close();
	}
	catch(const std::exception& e)
	{
		{
			boost::mutex::scoped_lock lock(s_LogMutex);

			std::cerr << "*	[" << connection_id << "]	exception: " << e.what() << std::endl;
		}

		if(sock->is_open())
			sock->close();
	}
}


int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("Allowed options");
	desc.add_options()
		("port",		po::value<short>(),			"port")
		("station",		po::value<std::string>(),	"data transfer station")
		("localid",		po::value<std::string>(),	"local user id")
		("interval",	po::value<unsigned long>()->implicit_value(400),	"directory lookup interval")
		("pitcher",		po::value<std::string>())
		("catcher",		po::value<std::string>())
		("udp_pitcher_host",		po::value<std::string>())
		("udp_pitcher_port",		po::value<std::string>())
		("udp_pitcher_interval",	po::value<unsigned long>())
		("udp_catcher_port",		po::value<unsigned short>())
		("video_pitcher_videoformat",	po::value<int>())
		("video_catcher_videoformat",	po::value<int>())
		("video_catcher_interval",		po::value<unsigned long>())
	;

	try
	{
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		const short port = vm["port"].as<short>();
		const std::string station = vm.count("station") ? vm["station"].as<std::string>() : "";
		const unsigned long interval = vm.count("interval") ? vm["interval"].as<unsigned long>() : s_DefaultInterval;

		const std::string pitchertype = vm.count("pitcher") ? vm["pitcher"].as<std::string>() : "FileSystem";
		const std::string catchertype = vm.count("catcher") ? vm["catcher"].as<std::string>() : "FileSystem";

		boost::asio::io_service io_service;

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
		else
			throw std::runtime_error("unknown catcher: " + catchertype);

		g_Bridge.reset(new Bridge(pitcher, catcher));

		if(vm.count("localid"))
			s_LocalId = vm["localid"].as<std::string>();

		{
			std::cout << "server started." << std::endl;

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
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Exception: " << e.what() << std::endl;
	}

	std::system("pause");

	return 0;
}
