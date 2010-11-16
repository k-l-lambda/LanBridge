
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "UdpBridgeCatcher.h"

#include <boost/algorithm/string.hpp>

#include <windows.h>


namespace UdpBridge
{
	static const size_t s_HeaderSize = 0x400;


	std::string getLine(const char* data, size_t length)
	{
		const size_t linelen = size_t(std::find(data, data + length, '\n') - data);

		return std::string(data, linelen);
	}


	Catcher::Catcher(size_t packsize, boost::asio::io_service& io_service, unsigned short port, unsigned long interval)
		: m_PackSize(packsize)
		, m_Interval(interval)
		, m_Socket(io_service, udp::endpoint(udp::v4(), port))
		, m_End(false)
		, m_ReceiveThread(boost::bind(&Catcher::receive, this))
	{
	}

	Catcher::~Catcher()
	{
		m_End = true;

		m_ReceiveThread.join();
	}

	size_t Catcher::read(const std::string& connection_id, char* buffer)
	{
		for(;;)
		{
			{
				boost::mutex::scoped_lock lock(m_DataMutex);

				DataQueue& queue = m_ConnectionDataMap[connection_id];
				if(!queue.empty())
				{
					DataBufferPtr data = queue.front();
					assert(data);
					if(!data->empty())
						std::memcpy(buffer, &(data->front()), data->size());

					queue.pop_front();

					return data->size();
				}
			}

			::Sleep(m_Interval);
		}

		// never get here
		assert(false);
	}

	void Catcher::acceptConnection(const AcceptorFunctor& acceptor)
	{
		boost::mutex::scoped_lock lock(m_NewConnectionsMutex);

		std::for_each(m_NewConnections.begin(), m_NewConnections.end(), acceptor);

		m_NewConnections.clear();
	}

	void Catcher::receive()
	{
		assert(!m_End);

		while(!m_End)
		{
			udp::endpoint sender_endpoint;

			boost::shared_array<char> data(new char[m_PackSize + s_HeaderSize]);
			const size_t length = m_Socket.receive_from(boost::asio::buffer(data.get(), m_PackSize + s_HeaderSize), sender_endpoint);

			const std::string header = getLine(data.get(), length);
			const std::string& connection_id = header;
			const char* body = data.get() + header.length() + 1;
			size_t body_length = length - (header.length() + 1);

			DataBufferPtr buffer(body_length ? new DataBuffer(body, body + body_length) : new DataBuffer);
			assert(buffer);

			//std::cout << "[" << connection_id << "] <" << sender_endpoint.address() << ">	received " << length << " bytes." << std::endl;

			{
				boost::mutex::scoped_lock lock(m_DataMutex);

				assert(buffer);
				m_ConnectionDataMap[connection_id].push_back(buffer);
			}

			if(connection_id[0] != '-')
			{
				boost::mutex::scoped_lock lock(m_NewConnectionsMutex);

				m_NewConnections.insert(connection_id);
			}
		}
	}
}
