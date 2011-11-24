

#ifndef	__MEMORYBRIDGEPITCHER_H__
#define	__MEMORYBRIDGEPITCHER_H__



#include "..\include\Bridge.h"

#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>

#include "Repository.h"


namespace MemoryBridge
{
	class Pitcher
		: public IPitcher
	{
	public:
		explicit Pitcher(const std::string& repository);

	private:
		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		Repository&					m_Repository;
	};
}



#endif // !defined(__MEMORYBRIDGEPITCHER_H__)
