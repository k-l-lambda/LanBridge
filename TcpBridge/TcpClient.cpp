
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpClient.h"

#include "..\include\Common.h"
#include "..\include\Log.h"

#include <boost/smart_ptr.hpp>


extern boost::asio::io_service io_service;


namespace TcpBridge
{
	TcpClient::TcpClient(const std::string& host, const std::string& port, const std::string& password)
		: m_Host(host)
		, m_Port(port)
		, m_Password(password)
	{
		connect();
	}

	void TcpClient::connect()
	{
		Log::shell(Log::Msg_Information) << "TcpClient startup, try to connect: " << m_Host << ":" << m_Port << ", " << m_Password;

		//boost::asio::io_service io_service;

		tcp::resolver resolver(io_service);
		boost::scoped_ptr<tcp::resolver::query> query;
		tcp::resolver::iterator iterator;

		try
		{
			query.reset(new tcp::resolver::query(tcp::v4(), m_Host, m_Port));
			iterator = resolver.resolve(*query);
		}
		catch(const boost::system::system_error& e)
		{
			Log::shell(Log::Msg_Warning) << "TcpClient resolve failed: " << e.what();

			throw e;
		}

		m_Socket.reset(new tcp::socket(io_service));

		for(; iterator != tcp::resolver::iterator(); ++iterator)
		{
			try
			{
				m_Socket->connect(*iterator);
				assert(m_Socket->is_open());

				Log::shell(Log::Msg_SetUp) << "TcpClient connection of \"" << m_Host << ":" << m_Port << "\" setup.";

				break;
			}
			catch(const std::exception& e)
			{
				Log::shell(Log::Msg_Warning) << "TcpClient request connect failed: " << e.what();

				throw e;
			}
		}

		// send password
		std::string buffer = m_Password + "\n";
		boost::asio::write(*m_Socket, boost::asio::buffer(buffer.c_str(), buffer.length()));
	}
}
