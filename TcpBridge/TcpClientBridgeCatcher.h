

#ifndef	__TCPCLIENTBRIDGECATCHER_H__
#define	__TCPCLIENTBRIDGECATCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>


namespace TcpClientBridge
{
	class Catcher
		: public ICatcher
	{
	public:
		Catcher(size_t packsize, boost::asio::io_service& io_service, unsigned long interval);
		~Catcher();

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);
	};
}



#endif // !defined(__TCPCLIENTBRIDGECATCHER_H__)
