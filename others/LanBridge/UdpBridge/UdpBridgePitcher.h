

#ifndef	__UDPBRIDGEPITCHER_H__
#define	__UDPBRIDGEPITCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>

using boost::asio::ip::udp;


namespace UdpBridge
{
	class Pitcher
		: public IPitcher
	{
	public:
		Pitcher(boost::asio::io_service& io_service, const std::string& host, const std::string& port, unsigned long interval);

	private:
		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		udp::socket					m_Socket;
		udp::resolver::iterator		m_Iterator;
		unsigned long				m_Interval;

		boost::mutex				m_SendMutex;
	};
}



#endif // !defined(__UDPBRIDGEPITCHER_H__)
