
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "MemoryBridgePitcher.h"


namespace MemoryBridge
{
	Pitcher::Pitcher(const std::string& repository)
		: m_Repository(RepositoryMap::instance()[repository])
	{
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		boost::mutex::scoped_lock lock(*m_Repository.Mutex);

		if(!m_Repository.Data.count(connection_id))
			m_Repository.NewConnections.insert(connection_id);

		m_Repository.Data[connection_id].push_back(MemoryBridge::DataBufferPtr(new DataBuffer(buffer, buffer + length)));
	}
}
