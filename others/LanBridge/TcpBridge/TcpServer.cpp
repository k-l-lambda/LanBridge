
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpServer.h"

#include "..\include\Common.h"


namespace TcpServerBridge
{
	TcpServer::TcpServer(unsigned short port, const std::string& password)
		: m_Port(port)
		, m_Password(password)
		, m_AcceptThread(boost::bind(&TcpServer::accept, this))
	{
	}

	TcpServer::~TcpServer()
	{
		m_AcceptThread.join();
	}

	void TcpServer::session(socket_ptr sock)
	{
	}

	void TcpServer::accept()
	{
		boost::asio::io_service io_service;

		tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), m_Port));
		for(;;)
		{
			socket_ptr sock(new tcp::socket(io_service));
			a.accept(*sock);
			boost::thread t(boost::bind(&TcpServer::session, this, sock));
		}
	}
}
