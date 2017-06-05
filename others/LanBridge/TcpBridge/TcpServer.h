

#ifndef	__TCPSERVER_H__
#define	__TCPSERVER_H__



#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/asio.hpp>


namespace TcpServerBridge
{
	class TcpServer
	{
	public:
		static boost::shared_ptr<TcpServer>& instance()
		{
			static boost::shared_ptr<TcpServer> p;
			return p;
		};

		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

	public:
		TcpServer(unsigned short port, const std::string& password);
		~TcpServer();

	private:
		void accept();
		void session(socket_ptr sock);

	private:
		unsigned short				m_Port;
		std::string					m_Password;

		socket_ptr					m_Socket;
		boost::thread				m_AcceptThread;
	};
}



#endif // !defined(__TCPSERVER_H__)
