
#ifndef	__BRIDGE_H__
#define	__BRIDGE_H__



#include <string>
#include <set>

#include <boost/smart_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>

//#include <windows.h>


struct IPitcher;
struct ICatcher;

typedef	boost::shared_ptr<IPitcher>		PitcherPtr;
typedef	boost::shared_ptr<ICatcher>		CatcherPtr;


struct IPitcher
{
	virtual void setCatcher(const CatcherPtr& catcher)	{};
	virtual void write(const std::string& connection_id, const char* buffer, size_t length)	= 0;
};


struct ICatcher
{
	virtual void setPitcher(const PitcherPtr& pitcher)	{};
	virtual size_t read(const std::string& connection_id, char* buffer)	= 0;

	typedef boost::function<void (const std::string&)>	AcceptorFunctor;
	virtual void acceptConnection(const AcceptorFunctor& acceptor)	= 0;
};


class Bridge
{
public:
	Bridge(const PitcherPtr& pitcher, const CatcherPtr& catcher, unsigned long interval = 20)
		: m_Pitcher(pitcher)
		, m_Catcher(catcher)
		, m_Interval(interval)
	{
		if(m_Pitcher)
			m_Pitcher->setCatcher(m_Catcher);
		if(m_Catcher)
			m_Catcher->setPitcher(m_Pitcher);
	};

	void write(const std::string& connection_id, const char* buffer, size_t length)
	{
		m_Pitcher->write(connection_id, buffer, length);
	};

	size_t read(const std::string& connection_id, char* buffer)
	{
		return m_Catcher->read(connection_id, buffer);
	};

	/*void acceptConnection(const ICatcher::AcceptorFunctor& acceptor)
	{
		return m_Catcher->acceptConnection(acceptor);
	};*/

	void acceptConnections(const ICatcher::AcceptorFunctor& acceptor)
	{
		for(;;)
		{
			m_Catcher->acceptConnection(boost::bind(&_l::accept, this, acceptor, _1));

#pragma warning(suppress:4996)
			_sleep(m_Interval);
		}
	};

private:
	struct _l
	{
		static void accept(Bridge* self, const ICatcher::AcceptorFunctor& acceptor, const std::string& connection_id)
		{
			if(!self->m_ActiveConnections.count(connection_id))
			{
				{
					boost::mutex::scoped_lock lock(self->m_ActiveConnectionsMutex);

					self->m_ActiveConnections.insert(connection_id);
				}

				boost::thread t(boost::bind(&Bridge::session, self, acceptor, connection_id));
			}
		};
	};

private:
	void session(const ICatcher::AcceptorFunctor& acceptor, const std::string& connection_id)
	{
		try
		{
			acceptor(connection_id);

			{
				boost::mutex::scoped_lock lock(m_ActiveConnectionsMutex);

				m_ActiveConnections.erase(connection_id);
			}
		}
		catch(const std::exception& e)
		{
			std::cerr << "*	[" << connection_id << "]	exception: " << e.what() << std::endl;
		}
	};

private:
	PitcherPtr	m_Pitcher;
	CatcherPtr	m_Catcher;

	unsigned long			m_Interval;

	std::set<std::string>	m_ActiveConnections;
	boost::mutex			m_ActiveConnectionsMutex;
};



#endif // !defined(__BRIDGE_H__)
