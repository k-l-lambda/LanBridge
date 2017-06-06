

#ifndef	__TCPCLIENTBRIDGECATCHER_H__
#define	__TCPCLIENTBRIDGECATCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>


namespace TcpBridge
{
	class Catcher
		: public ICatcher
	{
		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

	public:
		Catcher(const socket_ptr& socket, size_t packsize, unsigned long interval);
		~Catcher();

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		const socket_ptr&					m_Socket;

		size_t						m_PackSize;
		unsigned long				m_Interval;
	};
}



#endif // !defined(__TCPCLIENTBRIDGECATCHER_H__)
