
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpBridgePitcher.h"
#include "TcpServer.h"


namespace TcpBridge
{
	Pitcher::Pitcher(const socket_ptr& socket, unsigned long interval)
		: m_Socket(socket)
		, m_Interval(interval)
	{
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		if(m_Socket)
		{
			const std::string header = connection_id + "\n";

			/*boost::scoped_array<char> data(new char[header.length() + length]);
			std::memcpy(data.get(), header.data(), header.length());
			std::memcpy(data.get() + header.length(), buffer, length);*/

			boost::asio::write(*m_Socket, boost::asio::buffer(header.c_str(), header.length()));
			boost::asio::write(*m_Socket, boost::asio::buffer(buffer, length));

			::Sleep(m_Interval);
		}
	}
}
