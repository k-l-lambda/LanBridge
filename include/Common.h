
#ifndef	__COMMON_H__
#define	__COMMON_H__



#pragma warning(disable:4267)


#include <string>
#include <iostream>

#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#pragma warning(push, 1)
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#pragma warning(pop)

#include <windows.h>


using boost::asio::ip::tcp;

typedef boost::shared_ptr<tcp::socket> socket_ptr;

static const size_t s_PackLength = 0x2000;


inline void progressiveWait(boost::function<bool ()> condition, unsigned long start, unsigned long max, float step = 2)
{
	unsigned long interval = start;
	while(condition())
	{
		::Sleep(interval);

		if(interval < max)
			interval = unsigned long (interval * step);
	}
}

inline bool fileExistsOrFail(const std::string& path)
{
	try
	{
		return boost::filesystem::exists(path);
	}
	catch(const boost::system::system_error&)
	{
		return true;
	}
};

inline std::string getLine(const char* data, size_t length)
{
	const size_t linelen = size_t(std::find(data, data + length, '\n') - data);

	return std::string(data, linelen);
}

inline void outputDebugString(const std::string& message)
{
	OutputDebugStringA(message.c_str());
	OutputDebugStringA("\n");
}



#endif // !defined(__COMMON_H__)
