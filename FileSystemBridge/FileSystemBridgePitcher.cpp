
#include "FileSystemBridgePitcher.h"

#include <fstream>

#include <boost/filesystem.hpp>
//#include <boost/thread/mutex.hpp>

#include <windows.h>


namespace FileSystemBridge
{
	Pitcher::Pitcher(const std::string& station)
		: m_Station(station)
	{
		if(!boost::filesystem::exists(m_Station))
			boost::filesystem::create_directories(m_Station);
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		const std::string filename = m_Station + connection_id;

		// write file
		{
			std::ofstream file(filename.data(), std::ios::binary);
			if(!file.is_open())
				throw std::runtime_error("failed to open file: " + filename);

			if(length)
				file.write(buffer, std::streamsize(length));
		}

		const std::string targetfilename = filename + ".data";

		unsigned long interval = 100;
		while(boost::filesystem::exists(targetfilename))
		{
			::Sleep(interval);

			if(interval < 30000)
				interval = unsigned long (interval * 2);
		}

		boost::filesystem::rename(filename, targetfilename);
	}
}
