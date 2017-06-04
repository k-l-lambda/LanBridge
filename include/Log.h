
#ifndef	__LOG_H__
#define	__LOG_H__



#pragma warning(push)
#pragma warning(disable:4512)	// 'class' : assignment operator could not be generated
#pragma warning(disable:4819)	// The file contains a character that cannot be represented in the current code page (number). Save the file in Unicode format to prevent data loss.
#pragma warning(disable:6011)	// dereferencing NULL pointer <name>
#pragma warning(disable:6334)	// sizeof operator applied to an expression with an operator may yield unexpected results
#include <boost\shared_ptr.hpp>
#include <boost\function.hpp>
#include <boost\tuple\tuple.hpp>
#include <boost\thread\mutex.hpp>
#include <boost\assign.hpp>
#include <boost\lambda\lambda.hpp>
#include <boost\noncopyable.hpp>
#include <boost\date_time\posix_time\posix_time.hpp>
#pragma warning(pop)


#pragma warning(push)
#pragma warning(disable:4251)	// 'identifier' : class 'type' needs to have dll-interface to be used by clients of class 'type2'


namespace LogDetail
{
	static const char* const	s_LogMessageLevelSign[] =
	{
		"!",	// Msg_Fatal,
		"*",	// Msg_Warning,
		"c",	// Msg_Clew,
		"|",	// Msg_SetUp,
		"+",	// Msg_Plus,
		"-",	// Msg_Remove,
		">>",	// Msg_Input,
		"<<",	// Msg_Output,
		"i",	// Msg_Information,
	};

	static boost::mutex	s_LogMutex;
	static boost::mutex	s_IostreamMutex;


	static std::string	now()
	{
		return boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time()).substr(9);
	}
}


class Log
{
public:
	enum LogMessageLevel
	{
		Msg_Fatal,
		Msg_Warning,
		Msg_Clew,
		Msg_SetUp,
		Msg_Plus,
		Msg_Remove,
		Msg_Input,
		Msg_Output,
		Msg_Information,
	};

	enum LoggingMask
	{
		Mask_Fatal			= 1 << Msg_Fatal,
		Mask_Warning		= 1 << Msg_Warning,
		Mask_Clew			= 1 << Msg_Clew,
		Mask_SetUp			= 1 << Msg_SetUp,
		Mask_Plus			= 1 << Msg_Plus,
		Mask_Remove			= 1 << Msg_Remove,
		Mask_Input			= 1 << Msg_Input,
		Mask_Output			= 1 << Msg_Output,
		Mask_Information	= 1 << Msg_Information,

		Mask_Crucial	= Mask_Fatal | Mask_Warning | Mask_Clew,
		Mask_Normal		= Mask_Crucial | Mask_SetUp | Mask_Plus | Mask_Remove
							| Mask_Input | Mask_Output,
		Mask_Full		= ~0u,
	};


private:
	class Shell
		: public boost::noncopyable
	{
	public:
		explicit Shell(LogMessageLevel level)
			: m_Stream(new std::ostringstream)
			, m_StreamLock(new boost::mutex::scoped_lock(LogDetail::s_IostreamMutex))	// protect iostream multi-threads safe
			, m_Level(level)
		{
		};

		Shell(const Shell& s)
			: m_Stream(s.m_Stream)
			, m_StreamLock(s.m_StreamLock)
			, m_Level(s.m_Level)
		{
		};

		~Shell()
		{
			Log::getInstance().flush(m_Level, m_Stream->str());
		};


		template<typename T>
		std::ostream&	operator << (const T& x)
		{
			return (*m_Stream) << x;
		};

	public:
		static boost::mutex			s_Mutex;

	private:
		//std::stringstream	m_Stream;	// C1001, F:\SP\vctools\compiler\utc\src\P2\ehexcept.c(1453)
		boost::shared_ptr<boost::mutex::scoped_lock>	m_StreamLock;
		boost::shared_ptr<std::ostringstream>			m_Stream;
		LogMessageLevel									m_Level;
	};

public:
	typedef	boost::function<void (const std::string&)>					MessageFunctor;

	typedef	boost::tuples::tuple<MessageFunctor, LoggingMask, bool>		MessageListener;

private:
	Log()
		: m_ConsoleMask(Mask_Full)
	{
	};

public:
	static Log&		getInstance()
	{
		static Log s_Instance;

		return s_Instance;
	};

	static Shell	shell(LogMessageLevel level = Msg_Information)
	{
		return Log::Shell(level);
	};

public:
	void	setConsoleMask(LoggingMask mask)
	{
		m_ConsoleMask = mask;
	};

	void	addListener(const MessageFunctor& functor, LoggingMask mask = Mask_Normal, bool withtime = false)
	{
		m_Listeners.push_back(boost::tuples::make_tuple(functor, mask, withtime));
	};

//private:
	void	flush(LogMessageLevel level, const std::string& message)
	{
		using namespace LogDetail;

		boost::mutex::scoped_lock scoped_lock(LogDetail::s_LogMutex);


		std::string time = now();

		if(m_ConsoleMask & (1 << level))
		{
			std::clog << time << ' ' << s_LogMessageLevelSign[level] << '\t' << message << std::endl;
		}

		for(std::vector<MessageListener>::iterator it = m_Listeners.begin(); it != m_Listeners.end(); ++ it)
		{
			using boost::tuples::get;

			if(get<0>(*it) && (get<1>(*it) & (1 << level)))
			{
				std::string	logtext = (get<2>(*it) ? time + " " : "") + s_LogMessageLevelSign[level] + "\t"
					+ message;
				get<0>(*it)(logtext);
			}
		}
	};

private:
	LoggingMask		m_ConsoleMask;

	std::vector<MessageListener>	m_Listeners;
};


inline Log::LoggingMask	operator | (Log::LoggingMask left, Log::LoggingMask right)
{
	return Log::LoggingMask(int(left) | int(right));
}

inline Log::LoggingMask	operator & (Log::LoggingMask left, Log::LoggingMask right)
{
	return Log::LoggingMask(int(left) & int(right));
}


#pragma warning(pop)



#endif	// !defined(__LOG_H__)
