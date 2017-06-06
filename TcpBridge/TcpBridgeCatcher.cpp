
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpBridgeCatcher.h"


namespace TcpBridge
{
	Catcher::Catcher(const socket_ptr& socket, size_t packsize, unsigned long interval)
		: m_Socket(socket)
		, m_PackSize(packsize)
		, m_Interval(interval)
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
