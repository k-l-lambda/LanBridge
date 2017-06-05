
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpServer.h"

#include "..\include\Common.h"
#include "..\include\Log.h"


namespace TcpServerBridge
{
	TcpServer::TcpServer(unsigned short port, const std::string& password)
		: m_Port(port)
		, m_Password(password)
		, m_AcceptThread(boost::bind(&TcpServer::accept, this))
	{
		Log::shell(Log::Msg_Plus) << "TcpServer start.";
	}

	TcpServer::~TcpServer()
	{
		m_AcceptThread.join();

		Log::shell(Log::Msg_Remove) << "TcpServer end.";
	}

	void TcpServer::session(socket_ptr sock)
	{
		Log::shell(Log::Msg_Information) << "TcpServer session start, from: " << sock->remote_endpoint().address().to_string();
	}

	void TcpServer::accept()
	{
		boost::asio::io_service io_service;

		tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), m_Port));
		for(;;)
		{
			Log::shell(Log::Msg_Information) << "TcpServer startup at port: " << m_Port << ", password: " << m_Password;

			socket_ptr sock(new tcp::socket(io_service));
			a.accept(*sock);
			boost::thread t(boost::bind(&TcpServer::session, this, sock));
		}
	}
}
