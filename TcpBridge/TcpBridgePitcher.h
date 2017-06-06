

#ifndef	__TCPSERVERBRIDGEPITCHER_H__
#define	__TCPSERVERBRIDGEPITCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>


namespace TcpBridge
{
	class Pitcher
		: public IPitcher
	{
		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

	public:
		Pitcher(const socket_ptr& socket, unsigned long interval);

	private:
		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		const socket_ptr&			m_Socket;

		unsigned long				m_Interval;

		boost::mutex				m_SendMutex;
	};
}



#endif // !defined(__TCPSERVERBRIDGEPITCHER_H__)
