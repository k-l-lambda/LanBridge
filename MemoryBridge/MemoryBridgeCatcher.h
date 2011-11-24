

#ifndef	__MEMORYBRIDGECATCHER_H__
#define	__MEMORYBRIDGECATCHER_H__



#include "..\include\Bridge.h"

#include <deque>
#include <set>

#include <boost/asio.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

#include "Repository.h"


namespace MemoryBridge
{
	class Catcher
		: public ICatcher
	{
	public:
		Catcher(size_t packsize, unsigned long interval, const std::string& repository);
		~Catcher();

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		size_t						m_PackSize;
		unsigned long				m_Interval;

		Repository&					m_Repository;
	};
}



#endif // !defined(__MEMORYBRIDGECATCHER_H__)
