
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "MemoryBridgeCatcher.h"

#include <windows.h>


namespace MemoryBridge
{
	Catcher::Catcher(size_t packsize, unsigned long interval, const std::string& repository)
		: m_PackSize(packsize)
		, m_Interval(interval)
		, m_Repository(RepositoryMap::instance()[repository])
	{
	}

	Catcher::~Catcher()
	{
	}

	size_t Catcher::read(const std::string& connection_id, char* buffer)
	{
		for(;;)
		{
			{
				boost::mutex::scoped_lock lock(*m_Repository.Mutex);

				DataQueue& queue = m_Repository.Data[connection_id];
				if(!queue.empty())
				{
					DataBufferPtr data = queue.front();
					assert(data);
					if(!data->empty())
						std::memcpy(buffer, &(data->front()), data->size());

					queue.pop_front();

					// remove data queue when session end
					if(queue.empty() && data->empty())
						m_Repository.Data.erase(connection_id);

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
		boost::mutex::scoped_lock lock(*m_Repository.Mutex);

		std::for_each(m_Repository.NewConnections.begin(), m_Repository.NewConnections.end(), acceptor);

		m_Repository.NewConnections.clear();
	}
}
