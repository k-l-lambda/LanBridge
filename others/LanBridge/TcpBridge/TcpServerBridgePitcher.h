

#ifndef	__TCPSERVERBRIDGEPITCHER_H__
#define	__TCPSERVERBRIDGEPITCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>


namespace TcpServerBridge
{
	class Pitcher
		: public IPitcher
	{
	public:
		Pitcher(boost::asio::io_service& io_service, unsigned long interval);

	private:
		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		unsigned long				m_Interval;
	};
}



#endif // !defined(__TCPSERVERBRIDGEPITCHER_H__)
