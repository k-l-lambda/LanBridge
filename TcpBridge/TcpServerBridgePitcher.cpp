
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpServerBridgePitcher.h"


namespace TcpServerBridge
{
	Pitcher::Pitcher(boost::asio::io_service& io_service, unsigned long interval)
	{
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
	}
}
