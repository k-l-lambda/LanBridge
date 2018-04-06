
#include <string>

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

#include "..\include\Common.h"
#include "..\include\Log.h"



extern boost::asio::io_service io_service;


namespace LanBridgeServer
{
	void doParseHost(const std::string& request, std::string& host, std::string& port);
}


namespace Transfer
{
	socket_ptr setupOutSock(const std::string& host, const std::string& port)
	{
		socket_ptr sock(new tcp::socket(io_service));

		tcp::resolver resolver(io_service);
		boost::scoped_ptr<tcp::resolver::query> query;
		tcp::resolver::iterator iterator;

		try
		{
			query.reset(new tcp::resolver::query(tcp::v4(), host, port));
			iterator = resolver.resolve(*query);
		}
		catch (const boost::system::system_error& e)
		{
			if (e.code().value() == boost::asio::error::host_not_found)
			{
				// try again with ipv6
				query.reset(new tcp::resolver::query(tcp::v6(), host, port));
				iterator = resolver.resolve(*query);
			}
			else
				throw e;
		}

		for (; iterator != tcp::resolver::iterator(); ++iterator)
		{
			try
			{
				sock->connect(*iterator);
				assert(sock->is_open());

				return sock;
			}
			catch (const std::exception& e)
			{
				Log::shell(Log::Msg_Warning) << host << "	connect failed: " << e.what();
			}
			catch (const boost::system::error_code& e)
			{
				Log::shell(Log::Msg_Warning) << host << " connect failed: error_code: " << e.message();
			}
		}

		return sock;
	}


	static const size_t max_length = 0x20000;


	void session_input(const std::string& head, socket_ptr in_sock, socket_ptr out_sock)
	{
		while (in_sock->is_open())
		{
			char reply[max_length];
			boost::system::error_code error;

			size_t reply_length = out_sock->read_some(boost::asio::buffer(reply, max_length), error);
			if (error == boost::asio::error::eof || reply_length == 0)
			{
				if (error == boost::asio::error::eof)
					Log::shell(Log::Msg_Clew) << "EOF.";
				else if (reply_length == 0)
					Log::shell(Log::Msg_Clew) << "0 byte replied.";

				break;
			}

			if (reply_length)
				boost::asio::write(*in_sock, boost::asio::buffer(reply, reply_length));

			Log::shell(Log::Msg_Input) << "[" << head << "]" << "	received: " << reply_length << " bytes.";
		}

		if (out_sock->is_open())
			out_sock->close();
	}


	void session(socket_ptr in_sock, socket_ptr out_sock)
	{
		try
		{
			boost::scoped_ptr<boost::thread> inputThread;
			std::string head;

			Log::shell(Log::Msg_SetUp) << "connection setup from: " << in_sock->remote_endpoint().address();

			while (out_sock->is_open())
			{
				char data[max_length];
				boost::system::error_code error;
				size_t length = in_sock->read_some(boost::asio::buffer(data, max_length), error);
				if (error == boost::asio::error::eof)
				{
					Log::shell(Log::Msg_Clew) << "connection closed by peer.";
				}
				//else if (error)
				//	throw boost::system::system_error(error); // Some other error.

				if (head.empty())
				{
					const std::string text = std::string(data, length);

					try
					{
						std::string host, port;
						LanBridgeServer::doParseHost(text, host, port);

						head = host;
					}
					catch (const std::exception&)
					{
						Log::shell(Log::Msg_Clew) << "invalid head: " << text;
						head = "UNKNOWN";
					}

					//Log::shell(Log::Msg_Output) << head;
				}

				if (length)
					boost::asio::write(*out_sock, boost::asio::buffer(data, length));
				else {
					in_sock->close();
					break;
				}

				{
					//char* end = std::find(data, data + length, '\n');
					Log::shell(Log::Msg_Output) << "[" << head << "]" << "	sent: " << length << " bytes.";
				}

				if (!inputThread)
					inputThread.reset(new boost::thread(boost::bind(&session_input, head, in_sock, out_sock)));
			}

			if (in_sock->is_open())
				in_sock->close();

			if (inputThread)
				inputThread->join();
		}
		catch (const std::exception& e)
		{
			Log::shell(Log::Msg_Warning) << "transfer session exception: " << e.what();

			if (in_sock->is_open())
				in_sock->close();
			if (out_sock->is_open())
				out_sock->close();
		}
	}


	void runInteraction(const short listen, const std::string host, const std::string port)
	{
		Log::shell(Log::Msg_SetUp) << "Interaction started.";

		tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), listen));

		socket_ptr in_sock, out_sock;

		static const size_t max_length = 0x20000;
		char buffer[max_length];
		boost::system::error_code error;

		for (;;)
		{
			try
			{
				char input;
				std::cin >> input;

				switch (input)
				{
				case 'a':
					if (in_sock && in_sock->is_open())
						in_sock->close();
					if (out_sock && out_sock->is_open())
						out_sock->close();

					in_sock.reset(new tcp::socket(io_service));
					a.accept(*in_sock);

					out_sock = setupOutSock(host, port);

					Log::shell(Log::Msg_Information) << "connection accepted from: " << in_sock->remote_endpoint().address();

					break;
				case 'o':
					{
						size_t length = in_sock->read_some(boost::asio::buffer(buffer, max_length), error);
						if (error == boost::asio::error::eof)
						{
							Log::shell(Log::Msg_Clew) << "connection closed by peer.";
							break;
						}

						if (length)
							boost::asio::write(*out_sock, boost::asio::buffer(buffer, length));

						{
							Log::shell(Log::Msg_Output) << "sent: " << length << " bytes:";
							Log::shell(Log::Msg_Output) << std::string(buffer, length);
						}
					}

					break;
				case 'i':
					{
						size_t length = out_sock->read_some(boost::asio::buffer(buffer, max_length), error);
						if (error == boost::asio::error::eof || length == 0)
						{
							if (error == boost::asio::error::eof)
								Log::shell(Log::Msg_Clew) << "EOF.";
							else if (length == 0)
								Log::shell(Log::Msg_Clew) << "0 byte replied.";

							break;
						}

						if (length)
							boost::asio::write(*in_sock, boost::asio::buffer(buffer, length));

						Log::shell(Log::Msg_Input) << "received: " << length << " bytes:";
						Log::shell(Log::Msg_Input) << std::string(buffer, length);
					}

					break;
				default:
					Log::shell(Log::Msg_Information) << "unexpected input:" << input;
				}
			}
			catch (const std::exception& e)
			{
				Log::shell(Log::Msg_Warning) << "exception: " << e.what();
			}
		}
	}


	int main(int argc, char* argv[])
	{
		namespace po = boost::program_options;

		po::options_description desc("Allowed options");
		desc.add_options()
			("listen",			po::value<short>())
			("host",			po::value<std::string>())
			("port",			po::value<std::string>())
			("interactive",		po::value<bool>()->implicit_value(true))
			;

		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
		po::notify(vm);

		const short listen = vm["listen"].as<short>();
		const std::string host = vm["host"].as<std::string>();
		const std::string port = vm["port"].as<std::string>();


		if (vm.count("interactive"))
		{
			runInteraction(listen, host, port);
		}
		else
		{
			Log::shell(Log::Msg_SetUp) << "transfer started at " << listen << ", pipe to " << host << ":" << port;

			tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), listen));
			for (;;)
			{
				socket_ptr in_sock(new tcp::socket(io_service));
				a.accept(*in_sock);

				socket_ptr out_sock = setupOutSock(host, port);
				if (!out_sock)
					continue;

				boost::thread t(boost::bind(session, in_sock, out_sock));
			}
		}

		return 0;
	}
}
