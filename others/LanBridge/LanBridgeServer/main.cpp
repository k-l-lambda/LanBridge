
#pragma warning(disable:4267)

#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include <windows.h>

#include "..\include\Common.h"


using boost::asio::ip::tcp;

static const size_t max_length = 0x2000;

std::set<std::string> g_ActiveRequests;

std::string g_RequestsDir, g_ReponsesDir;

static boost::mutex s_LogMutex, s_ActiveRequestsMutex;


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

void parseHost(const std::string& request, std::string& host, std::string& port)
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
				//host = "94.23.221.49";
				//port = "80";

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


void writeResponse(const std::string& request, const char* buffer, size_t length)
{
	writeToStation(request, buffer, length, g_ReponsesDir, ".response");
}


void session_output(socket_ptr sock, const std::string& request)
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
					boost::mutex::scoped_lock lock(s_LogMutex);

					if(error == boost::asio::error::eof)
						std::cout << "EOF for \"" << request << "\"." << std::endl;
					else if(reply_length == 0)
						std::cout << "0 byte replied for \"" << request << "\"." << std::endl;
				}

				if(interval < 20000)
					interval *= 2;
				else
				{
					// end session
					writeResponse(request, reply, 0);

					break;
				}
				::Sleep(interval);

				continue;
			}

			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				std::cout << "Reply for \"" << request << "\", " << reply_length << " bytes received." << std::endl;
			}

			writeResponse(request, reply, reply_length);
		}
	}
	catch(const std::exception& e)
	{
		{
			boost::mutex::scoped_lock lock(s_LogMutex);

			std::cerr << "Exception in \"" << request << "\": " << e.what() << std::endl;
		}

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
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Unknown exception in \"" << request << "\": " << std::endl;
	}
}

void session(boost::asio::io_service& io_service, const std::string& request)
{
	try
	{
		socket_ptr sock(new tcp::socket(io_service));

		boost::scoped_ptr<boost::thread> outputthread;

		const std::string filename = g_RequestsDir + request + ".request";

		for(;;)
		{
			bool exist = false;
			for(unsigned long time = 0; time < 1200; ++time)
			{
				if(boost::filesystem::exists(filename))
				{
					exist = true;
					break;
				}

				/*{
					boost::mutex::scoped_lock lock(s_LogMutex);

					std::cout << "wait for client to commit next request of \"" << request << "\"." << std::endl;
				}*/

				::Sleep(1000);
			}
			if(!exist)
			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				std::cout << "wait for request \"" << request << "\" time out, end session." << std::endl;

				break;
			}

			std::ifstream file(filename.data(), std::ios::binary);
			if(!file.is_open())
				throw std::runtime_error("failed to open file: " + filename);

			std::vector<char> request_buffer(std::istreambuf_iterator<char>(file.rdbuf()), std::istreambuf_iterator<char>());

			// delete request file
			{
				file.close();
				boost::filesystem::remove(filename);
			}

			if(request_buffer.empty())
			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				std::cout << "0 byte request received, session of request \"" << request << "\" end." << std::endl;

				//throw std::runtime_error("request buffer is empty.");
				break;
			}

			const std::string command = parseCommand(&(request_buffer.front()));
			if(command.empty() && !sock->is_open())
			{
				boost::mutex::scoped_lock lock(s_LogMutex);

				std::cout << "error request header in request \"" << request << "\", session end." << std::endl;

				break;
			}

			if(!sock->is_open())
			{
				std::string host, port;
				parseHost(&(request_buffer.front()), host, port);

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

						{
							boost::mutex::scoped_lock lock(s_LogMutex);

							std::cout << "connection of \"" << host << ":" << port << "\" setup." << std::endl;
						}

						break;
					}
					catch(const std::exception& e)
					{
						boost::mutex::scoped_lock lock(s_LogMutex);

						std::cerr << "request \"" << request << "\" connect failed: " << e.what() << std::endl;
					}
				}
			}

			if(command == "CONNECT")
			{
				//static const std::string reply = "HTTP/1.1 100 Continue\r\n\r\n";
				static const std::string reply = "HTTP/1.1 200 OK\r\n\r\n";
				//static const std::string reply = "HTTP/1.1 204 No Content\r\n\r\n";

				writeResponse(request, reply.data(), reply.length());
			}
			else
			{
				boost::asio::write(*sock, boost::asio::buffer(&(request_buffer.front()), request_buffer.size()));

				if(!outputthread)
					outputthread.reset(new boost::thread(bind(&session_output, boost::ref(sock), request)));
			}
		}

		if(sock->is_open())
			sock->close();

		// remove request in g_ActiveRequests
		{
			boost::mutex::scoped_lock lock(s_ActiveRequestsMutex);

			g_ActiveRequests.erase(request);
		}
	}
	catch(const std::exception& e)
	{
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Exception in \"" << request << "\": " << e.what() << std::endl;
	}
	catch(...)
	{
		boost::mutex::scoped_lock lock(s_LogMutex);

		std::cerr << "Unknown exception in \"" << request << "\": " << std::endl;
	}
}


int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("Allowed options");
	desc.add_options()
		("station",		po::value<std::string>(),							"data transfer station")
		("interval",	po::value<unsigned long>()->implicit_value(400),	"directory lookup interval")
	;

	try
	{
		po::variables_map vm;
		po::store(po::parse_command_line(argc, argv, desc), vm);
		po::notify(vm);

		const std::string station = vm["station"].as<std::string>();
		g_RequestsDir = station + "\\requests\\";
		g_ReponsesDir = station + "\\responses\\";
		unsigned long interval = vm.count("interval") ? vm["interval"].as<unsigned long>() : 400;

		if(!boost::filesystem::exists(g_RequestsDir))
			boost::filesystem::create_directories(g_RequestsDir);
		if(!boost::filesystem::exists(g_ReponsesDir))
			boost::filesystem::create_directories(g_ReponsesDir);

		boost::asio::io_service io_service;

		for(;;)
		{
			const boost::filesystem::directory_iterator end_it;
			for(boost::filesystem::directory_iterator it(g_RequestsDir); it != end_it; ++ it)
			{
				if(boost::filesystem::is_regular_file(it->status()))
				{
					const std::string extension = boost::to_lower_copy(it->path().extension());
					if(extension == ".request")
					{
						const std::string request = it->path().stem();
						if(!g_ActiveRequests.count(request))
						{
							{
								boost::mutex::scoped_lock lock(s_ActiveRequestsMutex);

								g_ActiveRequests.insert(request);
							}
							boost::thread t(boost::bind(session, boost::ref(io_service), request));
						}
					}
				}
			}

			::Sleep(interval);
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cerr << "Unknown exception." << std::endl;
	}

	std::system("pause");

	return 0;
}
