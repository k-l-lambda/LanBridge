
#include <string>
#include <iostream>

#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>

#include <windows.h>


using boost::asio::ip::tcp;

typedef boost::shared_ptr<tcp::socket> socket_ptr;


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


inline void writeToStation(const std::string& request, const char* buffer, size_t length, const std::string& dir, const char* ext)
{
	static boost::mutex s_Mutex;
	boost::mutex::scoped_lock lock(s_Mutex);

	const std::string filename = dir + request;
	std::ofstream file(filename.data(), std::ios::binary);
	if(!file.is_open())
		throw std::runtime_error("failed to open file: " + filename);

	if(length)
		file.write(buffer, length);
	file.close();

	const std::string targetfilename = filename + ext;

	progressiveWait(boost::bind((bool (*)(const boost::filesystem::path &))&boost::filesystem::exists, targetfilename), 100, 30000);

	boost::filesystem::rename(filename, targetfilename);
}
