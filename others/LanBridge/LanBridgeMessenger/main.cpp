
#pragma warning(disable:4267)

#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>
#include <fstream>


#include <boost/program_options.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>

#include "..\FileSystemBridge\FileSystemBridgePitcher.h"
#include "..\FileSystemBridge\FileSystemBridgeCatcher.h"
#pragma comment(lib, "FileSystemBridge.lib")

#include "..\UdpBridge\UdpBridgePitcher.h"
#include "..\UdpBridge\UdpBridgeCatcher.h"
#pragma comment(lib, "UdpBridge.lib")

#include "..\VideoBridge\VideoBridgePitcher.h"
#include "..\VideoBridge\VideoBridgeCatcher.h"
#pragma comment(lib, "VideoBridge.lib")


namespace po = boost::program_options;
using boost::asio::ip::udp;
namespace filesystem = boost::filesystem;

const short s_DefaultPort = 81;
const std::string s_ReceivedDirectory = ".\\received\\";

static const size_t max_length = 0x4000;


std::string getLine(const char* data, size_t length)
{
	const size_t linelen = size_t(std::find(data, data + length, '\n') - data);

	return std::string(data, linelen);
}

std::string genConnectionId()
{
	std::srand(std::clock());
	static size_t index = 0;

	return boost::lexical_cast<std::string>(std::rand() + (index++));
}


void server(po::variables_map& vm)
{
	const std::string pitchertype = vm.count("pitcher") ? vm["pitcher"].as<std::string>() : "";
	const std::string catchertype = vm.count("catcher") ? vm["catcher"].as<std::string>() : "UDP";

	PitcherPtr pitcher;
	if(pitchertype == "FileSystem")
	{
		const std::string fs_pitcher_station = vm["fs_pitcher_station"].as<std::string>();

		pitcher.reset(new FileSystemBridge::Pitcher(fs_pitcher_station));
	}
	else if(pitchertype == "UDP")
	{
		const std::string udp_pitcher_host = vm["udp_pitcher_host"].as<std::string>();
		const std::string udp_pitcher_port = vm["udp_pitcher_port"].as<std::string>();
		const unsigned long udp_pitcher_interval = vm.count("udp_pitcher_interval") ? vm["udp_pitcher_interval"].as<unsigned long>() : 10;

		static boost::asio::io_service io_service;

		pitcher.reset(new UdpBridge::Pitcher(io_service, udp_pitcher_host, udp_pitcher_port, udp_pitcher_interval));
	}
	else if(pitchertype == "Video")
	{
		const size_t video_pitcher_framewidth = vm.count("video_pitcher_framewidth") ? vm["video_pitcher_framewidth"].as<size_t>() : 128;
		const size_t video_pitcher_frameheight = vm.count("video_pitcher_frameheight") ? vm["video_pitcher_frameheight"].as<size_t>() : 128;
		const int video_pitcher_videoformat = vm.count("video_pitcher_videoformat") ? vm["video_pitcher_videoformat"].as<int>() : VideoBridge::VF_24bits;

		pitcher.reset(new VideoBridge::Pitcher(video_pitcher_framewidth, video_pitcher_frameheight, VideoBridge::VideoFormat(video_pitcher_videoformat)));
	}
	else
		throw std::runtime_error("unknown pitcher: " + pitchertype);

	CatcherPtr catcher;
	if(catchertype == "FileSystem")
	{
		const std::string fs_catcher_station = vm["fs_catcher_station"].as<std::string>();

		catcher.reset(new FileSystemBridge::Catcher(max_length, fs_catcher_station, 400));
	}
	else if(catchertype == "UDP")
	{
		const unsigned short udp_catcher_port = vm["udp_catcher_port"].as<unsigned short>();

		static boost::asio::io_service io_service;

		catcher.reset(new UdpBridge::Catcher(max_length, io_service, udp_catcher_port, 400));
	}
	else if(catchertype == "Video")
	{
		const int video_catcher_videoformat = vm.count("video_catcher_videoformat") ? vm["video_catcher_videoformat"].as<int>() : VideoBridge::VF_24bits;

		catcher.reset(new VideoBridge::Catcher(max_length, 200, VideoBridge::VideoFormat(video_catcher_videoformat)));
	}
	else
		throw std::runtime_error("unknown catcher: " + catchertype);

	struct _l
	{
		static void session(Bridge* bridge, const std::string& connection_id)
		{
			char data[max_length];
			const size_t length = bridge->read(connection_id, data);
			if(!length)
			{
				std::cout << "empty data, drop." << std::endl;
				return;
			}

			const std::string command = getLine(data, length);
			const char* body = data + command.length() + 1;
			size_t body_length = length - (command.length() + 1);

			if(command == "MESSAGE")
			{
				std::cout << "message recived from " << connection_id << ":" << std::endl;
				std::cout.write(body, body_length);
				std::cout << std::endl;
			}
			else if(command == "FILE")
			{
				const std::string filename = getLine(body, body_length);
				body += filename.length() + 1;
				body_length -= filename.length() + 1;

				std::cout << "file \"" << filename << "\" recived from " << connection_id << "." << std::endl;

				size_t packs = 0;
				{
					if(!boost::filesystem::exists(s_ReceivedDirectory))
						boost::filesystem::create_directories(s_ReceivedDirectory);
					std::ofstream file((s_ReceivedDirectory + filename).data(), std::ios::binary);
					if(body_length)
						file.write(body, body_length);
					else
						body_length = 1;

					//std::cout << "body_length: " << body_length << std::endl;
					while(body_length)
					{
						body_length = bridge->read(connection_id, data);
						if(body_length)
						{
							file.write(data, body_length);

							++packs;
						}
					}
				}

				std::cout << "file receive end, " << packs << " packs in total." << std::endl;
			}
			else
				throw std::runtime_error("unknown command: " + command);
		};
	};

	std::cout << "LanBridgeMessenger server starup." << std::endl;

	Bridge bridge(pitcher, catcher);
	bridge.acceptConnections(boost::bind(&_l::session, &bridge, _1));
}


void client(po::variables_map& vm)
{
	const std::string pitchertype = vm.count("pitcher") ? vm["pitcher"].as<std::string>() : "UDP";
	const std::string catchertype = vm.count("catcher") ? vm["catcher"].as<std::string>() : "";

	PitcherPtr pitcher;
	if(pitchertype == "FileSystem")
	{
		const std::string fs_pitcher_station = vm["fs_pitcher_station"].as<std::string>();

		pitcher.reset(new FileSystemBridge::Pitcher(fs_pitcher_station));
	}
	else if(pitchertype == "UDP")
	{
		const std::string udp_pitcher_host = vm["udp_pitcher_host"].as<std::string>();
		const std::string udp_pitcher_port = vm["udp_pitcher_port"].as<std::string>();
		const unsigned long udp_pitcher_interval = vm.count("udp_pitcher_interval") ? vm["udp_pitcher_interval"].as<unsigned long>() : 10;

		static boost::asio::io_service io_service;

		pitcher.reset(new UdpBridge::Pitcher(io_service, udp_pitcher_host, udp_pitcher_port, udp_pitcher_interval));
	}
	else if(pitchertype == "Video")
	{
		const size_t video_pitcher_framewidth = vm.count("video_pitcher_framewidth") ? vm["video_pitcher_framewidth"].as<size_t>() : 128;
		const size_t video_pitcher_frameheight = vm.count("video_pitcher_frameheight") ? vm["video_pitcher_frameheight"].as<size_t>() : 128;
		const int video_pitcher_videoformat = vm.count("video_pitcher_videoformat") ? vm["video_pitcher_videoformat"].as<int>() : VideoBridge::VF_24bits;

		pitcher.reset(new VideoBridge::Pitcher(video_pitcher_framewidth, video_pitcher_frameheight, VideoBridge::VideoFormat(video_pitcher_videoformat)));
	}
	else
		throw std::runtime_error("unknown pitcher: " + pitchertype);

	CatcherPtr catcher;
	if(catchertype == "FileSystem")
	{
		const std::string fs_catcher_station = vm["fs_catcher_station"].as<std::string>();

		catcher.reset(new FileSystemBridge::Catcher(max_length, fs_catcher_station, 400));
	}
	else if(catchertype == "UDP")
	{
		const unsigned short udp_catcher_port = vm["udp_catcher_port"].as<unsigned short>();

		static boost::asio::io_service io_service;

		catcher.reset(new UdpBridge::Catcher(max_length, io_service, udp_catcher_port, 400));
	}
	else if(!catchertype.empty())
		throw std::runtime_error("unknown catcher: " + catchertype);

	Bridge bridge(pitcher, catcher);


	if(vm.count("message"))
	{
		const std::string message = vm["message"].as<std::string>();
		const std::string data = "MESSAGE\n" + message;

		bridge.write(genConnectionId(), data.data(), data.length());

		std::cout << "message sent." << std::endl;
	}
	else if(vm.count("file"))
	{
		const std::string filename = vm["file"].as<std::string>();

		std::ifstream file(filename.data(), std::ios::binary);
		if(file.is_open())
		{
			const std::string connection_id = genConnectionId();

			std::string header = "FILE\n" + filesystem::path(filename).filename() + "\n";

			bridge.write(connection_id, header.data(), header.length());

			size_t packs = 0;
			while(!file.eof())
			{
				char data[max_length];
				file.read(data, max_length);
				const size_t length = file.gcount();
				bridge.write(connection_id, data, length);

				++packs;
			}
			bridge.write(connection_id, header.data(), 0);


			std::cout << "file sent, " << packs << " packs in total." << std::endl;
		}
		else
			throw std::runtime_error("cannot find file: " + filename);
	}
}


int main(int argc, char* argv[])
{
	po::options_description desc("Allowed options");
	desc.add_options()
		("server",						"startup as server")
		("message",						po::value<std::string>())
		("file",						po::value<std::string>())
		("pitcher",						po::value<std::string>())
		("catcher",						po::value<std::string>())
		("fs_pitcher_station",			po::value<std::string>())
		("fs_catcher_station",			po::value<std::string>())
		("udp_pitcher_host",			po::value<std::string>())
		("udp_pitcher_port",			po::value<std::string>())
		("udp_pitcher_interval",		po::value<unsigned long>())
		("udp_catcher_port",			po::value<unsigned short>())
		("video_pitcher_framewidth",	po::value<size_t>())
		("video_pitcher_frameheight",	po::value<size_t>())
		("video_pitcher_videoformat",	po::value<int>())
		("video_catcher_videoformat",	po::value<int>())
	;

	try
	{
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		if(vm.count("server"))
			server(vm);
		else
			client(vm);
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cerr << "Unknown exception." << std::endl;
	}

	return 0;
}
