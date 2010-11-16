
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "UdpBridgePitcher.h"


namespace UdpBridge
{
	Pitcher::Pitcher(boost::asio::io_service& io_service, const std::string& host, const std::string& port, unsigned long interval)
		: m_Socket(io_service, udp::endpoint(udp::v4(), 0))
		, m_Interval(interval)
	{
		udp::resolver resolver(io_service);
		udp::resolver::query query(udp::v4(), host, port);
		m_Iterator = resolver.resolve(query);
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		const std::string header = connection_id + "\n";

		boost::scoped_array<char> data(new char[header.length() + length]);
		std::memcpy(data.get(), header.data(), header.length());
		std::memcpy(data.get() + header.length(), buffer, length);

		{
			boost::mutex::scoped_lock lock(m_SendMutex);

			m_Socket.send_to(boost::asio::buffer(data.get(), header.length() + length), *m_Iterator);

			::Sleep(m_Interval);
		}
	}
}
