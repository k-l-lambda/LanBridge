
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


using boost::asio::ip::tcp;

static const size_t max_length = 0x2000;

typedef boost::shared_ptr<tcp::socket> socket_ptr;

static boost::mutex s_LogMutex;

std::string g_RequestsDir, g_ReponsesDir;


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

std::string genRequestName()
{
	std::srand(std::clock());
	static size_t index = 0;

	return s_LocalId + "_" + boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time()) + "_" + boost::lexical_cast<std::string>(std::rand() + (index++));
}

std::string now()
{
	return boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time());
}


unsigned long	g_Interval = 400;

static std::ofstream s_RequestsLog("requests.log", std::ios::app);
static std::ofstream s_ResponsesLog("responses.log", std::ios::app);


void writeRequest(const std::string& request, const char* buffer, size_t length)
{
	writeToStation(request, buffer, length, g_RequestsDir, ".request");
}


void session_input(socket_ptr sock, const std::string& request_name)
{
	try
	{
		while(sock->is_open())
		{
			const std::string response_filename = g_ReponsesDir + request_name + ".response";
			std::ifstream response_file(response_filename.data(), std::ios::binary);
			if(response_file.is_open())
			{
				std::vector<char> response_buffer(std::istreambuf_iterator<char>(response_file.rdbuf()), std::istreambuf_iterator<char>());

				// delete response file
				{
					response_file.close();
					boost::filesystem::remove(response_filename);
				}

				if(!response_buffer.empty())
				{
					boost::asio::write(*sock, boost::asio::buffer(&(response_buffer.front()), response_buffer.size()));

					{
						boost::mutex::scoped_lock lock(s_LogMutex);

						s_ResponsesLog << std::endl << "[" << now() << "]\t" << "response " << request_name << ":" << std::endl;
						s_ResponsesLog.write(&(response_buffer.front()), response_buffer.size());
						s_ResponsesLog << std::endl;
					}
				}
				else
				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::cerr << "->	response of \"" << request_name << "\" is empty, session end." << std::endl;

					break;
				}

				{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::cout << "->	response of \"" << request_name << "\": " << response_buffer.size() << " bytes." << std::endl;
				}
			}
			else
				::Sleep(g_Interval);
		}
	}
	catch(const std::exception& e)
	{
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Exception in \"" << request_name << "\": " << e.what() << std::endl;
	}

	if(sock->is_open())
		sock->close();
}


void session(socket_ptr sock)
{
	assert(s_RequestsLog.is_open());
	assert(s_ResponsesLog.is_open());

	const std::string request_name = genRequestName();

	try
	{
		const std::string filename = g_RequestsDir + request_name;

		boost::thread inputthread(boost::bind(&session_input, sock, request_name));

		while(sock->is_open())
		{
			char data[max_length];
			boost::system::error_code error;
			size_t length = sock->read_some(boost::asio::buffer(data, max_length), error);
			if(error == boost::asio::error::eof)
				break; // Connection closed cleanly by peer.
				//return; // Connection closed cleanly by peer.
			else if(error)
				throw boost::system::system_error(error); // Some other error.

			writeRequest(request_name, data, length);

			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				char* end = std::find(data, data + length, '\n');
				std::cout << "<-	request \"" << request_name << "\" sent: " << length << " bytes: " << std::string(data, end - data) << std::endl;
			}

			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				s_RequestsLog << std::endl << "[" << now() << "]\t" << "request " << request_name << ":" << std::endl;
				s_RequestsLog.write(data, length);
				s_RequestsLog << std::endl;
			}
		}

		writeRequest(request_name, NULL, NULL);

		if(sock->is_open())
			sock->close();
	}
	catch(const std::exception& e)
	{
		{
			boost::mutex::scoped_lock lock(s_LogMutex);

			std::cerr << "Exception in \"" << request_name << "\": " << e.what() << std::endl;
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
	;

	try
	{
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		const short port = vm["port"].as<short>();
		const std::string station = vm["station"].as<std::string>();
		g_RequestsDir = station + "\\requests\\";
		g_ReponsesDir = station + "\\responses\\";
		if(vm.count("interval"))
			g_Interval = vm["interval"].as<unsigned long>();

		if(vm.count("localid"))
			s_LocalId = vm["localid"].as<std::string>();

		{
			boost::asio::io_service io_service;

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
	catch (std::exception& e)
	{
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Exception: " << e.what() << std::endl;
	}

	std::system("pause");

	return 0;
}
