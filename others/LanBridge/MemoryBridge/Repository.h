

#ifndef	__MEMORYBRIDGEREPOSITORY_H__
#define	__MEMORYBRIDGEREPOSITORY_H__



#include <vector>
#include <deque>
#include <string>
#include <map>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>


namespace MemoryBridge
{
	typedef	std::vector<char>						DataBuffer;
	typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;
	typedef	std::deque<DataBufferPtr>				DataQueue;
	typedef	std::map<std::string, DataQueue>		ConnectionDataMap;

	struct Repository
	{
		ConnectionDataMap				Data;
		boost::shared_ptr<boost::mutex>	Mutex;
		std::set<std::string>			NewConnections;

		Repository()
			: Mutex(new boost::mutex)
		{
		};
	};

	struct RepositoryMap
		: public std::map<std::string, Repository>
	{
		static RepositoryMap& instance()
		{
			static RepositoryMap map;
			return map;
		};
	};
}



#endif // !defined(__MEMORYBRIDGEREPOSITORY_H__)
