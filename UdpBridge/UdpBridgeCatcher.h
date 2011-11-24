

#ifndef	__UDPBRIDGECATCHER_H__
#define	__UDPBRIDGECATCHER_H__



#include "..\include\Bridge.h"

#include <deque>
#include <set>

#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>


namespace UdpBridge
{
	using boost::asio::ip::udp;


	class Catcher
		: public ICatcher
	{
	public:
		Catcher(size_t packsize, boost::asio::io_service& io_service, unsigned short port, unsigned long interval);
		~Catcher();

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		void receive();

	private:
		size_t						m_PackSize;
		unsigned long				m_Interval;

		udp::socket					m_Socket;

		typedef	std::vector<char>						DataBuffer;
		typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;
		typedef	std::deque<DataBufferPtr>				DataQueue;
		typedef	std::map<std::string, DataQueue>		ConnectionDataMap;

		ConnectionDataMap			m_ConnectionDataMap;
		boost::mutex				m_DataMutex;

		std::set<std::string>		m_NewConnections;
		boost::mutex				m_NewConnectionsMutex;

		bool						m_End;
		boost::thread				m_ReceiveThread;
	};
}



#endif // !defined(__UDPBRIDGECATCHER_H__)
