
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpClientBridgeCatcher.h"


namespace TcpClientBridge
{
	Catcher::Catcher(size_t packsize, boost::asio::io_service& io_service, unsigned long interval)
	{
	}

	Catcher::~Catcher()
	{
	}

	size_t Catcher::read(const std::string& connection_id, char* buffer)
	{
		return 0;
	}

	void Catcher::acceptConnection(const AcceptorFunctor& acceptor)
	{
	}
}
