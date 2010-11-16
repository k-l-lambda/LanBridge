

#ifndef	__FILESYSTEMBRIDGECATCHER_H__
#define	__FILESYSTEMBRIDGECATCHER_H__



#include "..\include\Bridge.h"


namespace FileSystemBridge
{
	class Catcher
		: public ICatcher
	{
	public:
		Catcher(size_t packsize, const std::string& station, unsigned long interval);

	private:
		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		size_t			m_PackSize;
		std::string		m_Station;
		unsigned long	m_Interval;
	};
}



#endif // !defined(__FILESYSTEMBRIDGECATCHER_H__)
