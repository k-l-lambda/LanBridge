
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpBridgePitcher.h"
#include "TcpServer.h"

#include "..\include\Base64Helper.h"


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
			/*const std::string header = connection_id + "\n";

			boost::scoped_array<char> data(new char[header.length() + length]);
			std::memcpy(data.get(), header.data(), header.length());
			std::memcpy(data.get() + header.length(), buffer, length);*/
			std::string data = connection_id + "\n";
			data += MUtils::Base64Helper::encode(buffer, length) + "\n\n";

			{
				boost::mutex::scoped_lock lock(m_SendMutex);

				boost::asio::write(*m_Socket, boost::asio::buffer(data.c_str(), data.size()));

				Log::shell(Log::Msg_Information) << "TcpBridgePitcher	data for [" << connection_id << "] sent.";

				::Sleep(m_Interval);
			}
		}
	}
}
