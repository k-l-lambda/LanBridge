
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
					{
						if(data->size() > m_PackSize)
							Log::shell(Log::Msg_Information) << "TcpBridgeCatcher	data size (" << data->size() << ") is out of pack size (" << m_PackSize << ").";

						std::memcpy(buffer, &(data->front()), std::min(data->size(), m_PackSize));
					}

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
				static const size_t s_HeaderSize = 0x80;

				boost::shared_array<char> data(new char[m_PackSize + s_HeaderSize]);
				boost::system::error_code error;
				size_t length = m_Socket->read_some(boost::asio::buffer(data.get(), m_PackSize + s_HeaderSize), error);
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

				const std::string header = getLine(data.get(), length);
				const std::string& connection_id = header;
				const char* body = data.get() + header.length() + 1;
				size_t body_length = length - std::min((header.length() + 1), length);

				Log::shell(Log::Msg_Information) << "TcpBridgeCatcher	[" << header.c_str() << "]	" << length << " bytes received.";

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
