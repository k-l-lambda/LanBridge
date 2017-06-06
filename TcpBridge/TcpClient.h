

#ifndef	__TCPCLIENT_H__
#define	__TCPCLIENT_H__



#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>


namespace TcpClientBridge
{
	class TcpClient
	{
	public:
		static boost::shared_ptr<TcpClient>& instance()
		{
			static boost::shared_ptr<TcpClient> p;
			return p;
		};

		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

	public:
		TcpClient(const std::string& host, const std::string& port, const std::string& password);

		socket_ptr	getSocket() const	{return m_Socket;};

	private:
		void connect();

	private:
		std::string		m_Host;
		std::string		m_Port;
		std::string		m_Password;

		socket_ptr		m_Socket;
	};
}



#endif // !defined(__TCPCLIENT_H__)
