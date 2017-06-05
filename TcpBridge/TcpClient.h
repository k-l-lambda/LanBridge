

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

	public:
		TcpClient(const std::string& host, const std::string& port, const std::string& password);
	};
}



#endif // !defined(__TCPCLIENT_H__)
