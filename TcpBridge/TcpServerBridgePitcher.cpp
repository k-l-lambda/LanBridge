
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpServerBridgePitcher.h"
#include "TcpServer.h"


namespace TcpServerBridge
{
	Pitcher::Pitcher(boost::asio::io_service& io_service, unsigned long interval)
		: m_Interval(interval)
	{
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		if(TcpServer::instance())
		{
			TcpServer::socket_ptr socket = TcpServer::instance()->getSocket();
			if(socket)
			{
				const std::string header = connection_id + "\n";

				/*boost::scoped_array<char> data(new char[header.length() + length]);
				std::memcpy(data.get(), header.data(), header.length());
				std::memcpy(data.get() + header.length(), buffer, length);*/

				boost::asio::write(*socket, boost::asio::buffer(header.c_str(), header.length()));
				boost::asio::write(*socket, boost::asio::buffer(buffer, length));

				::Sleep(m_Interval);
			}
		}
	}
}
