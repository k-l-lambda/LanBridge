

#ifndef	__TCPCLIENTBRIDGECATCHER_H__
#define	__TCPCLIENTBRIDGECATCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>


namespace TcpBridge
{
	class Catcher
		: public ICatcher
	{
		typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

		typedef	std::vector<char>						DataBuffer;
		typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;
		typedef	std::deque<DataBufferPtr>				DataQueue;
		typedef	std::map<std::string, DataQueue>		ConnectionDataMap;

	public:
		Catcher(const socket_ptr& socket, size_t packsize, unsigned long interval);
		~Catcher();

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		void receive();

		void updateReceive();
		void pushConnection(const std::string& header, const DataBufferPtr& body);

	private:
		const socket_ptr&			m_Socket;

		size_t						m_PackSize;
		unsigned long				m_Interval;

		ConnectionDataMap			m_ConnectionDataMap;
		boost::mutex				m_DataMutex;

		std::set<std::string>		m_NewConnections;
		boost::mutex				m_NewConnectionsMutex;

		bool						m_End;
		boost::thread				m_ReceiveThread;

		std::string					m_ReceiveBuffer;
		std::string					m_CurrentHeader;
	};
}



#endif // !defined(__TCPCLIENTBRIDGECATCHER_H__)
