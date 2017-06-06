
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "TcpBridgeCatcher.h"

#include "..\include\Common.h"


namespace TcpBridge
{
	Catcher::Catcher(const socket_ptr& socket, size_t packsize, unsigned long interval)
		: m_Socket(socket)
		, m_PackSize(packsize)
		, m_Interval(interval)
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
			if(m_Socket)
			{
				static const size_t max_length = 0x2000;

				char read_buf[max_length];
				boost::system::error_code error;
				size_t length = m_Socket->read_some(boost::asio::buffer(read_buf, max_length), error);
				if(error == boost::asio::error::eof || length == 0)
				{
					{
						if(error == boost::asio::error::eof)
							Log::shell(Log::Msg_Clew) << "TcpBridgeCatcher	EOF.";
						//else if(length == 0)
						//	Log::shell(Log::Msg_Clew) << "TcpBridgeCatcher	0 byte received.";
					}

					::Sleep(m_Interval);

					continue;
				}

				Log::shell(Log::Msg_Information) << "TcpBridgeCatcher	" << length << " bytes received.";

				const std::string header = getLine(read_buf, length);
				const std::string& connection_id = header;
				const char* body = read_buf + header.length() + 1;
				size_t body_length = length - std::min((header.length() + 1), length);

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
			else
			{
				Log::shell(Log::Msg_Information) << "TcpBridgeCatcher	socket is null, sleep.";

				::Sleep(m_Interval);
			}
		}
	}
}
