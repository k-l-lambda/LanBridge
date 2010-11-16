
#include "FileSystemBridgeCatcher.h"

#include <fstream>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <windows.h>


namespace FileSystemBridge
{
	Catcher::Catcher(size_t packsize, const std::string& station, unsigned long interval)
		: m_PackSize(packsize)
		, m_Station(station)
		, m_Interval(interval)
	{
		if(!boost::filesystem::exists(m_Station))
			boost::filesystem::create_directories(m_Station);
	}

	size_t Catcher::read(const std::string& connection_id, char* buffer)
	{
		const std::string filename = m_Station + connection_id + ".data";
		while(!boost::filesystem::exists(filename))
			::Sleep(m_Interval);

		std::ifstream file(filename.data(), std::ios::binary);
		if(!file.is_open())
		{
			throw std::runtime_error("cannot open file: " + filename);
		}

		//std::vector<char> response_buffer(std::istreambuf_iterator<char>(file.rdbuf()), std::istreambuf_iterator<char>());
		file.read(buffer, std::streamsize(m_PackSize));
		const size_t length = file.gcount();

		// delete data file
		{
			file.close();
			boost::filesystem::remove(filename);
		}

		return length;
	}

	void Catcher::acceptConnection(const AcceptorFunctor& acceptor)
	{
		const boost::filesystem::directory_iterator end_it;
		for(boost::filesystem::directory_iterator it(m_Station); it != end_it; ++ it)
		{
			if(boost::filesystem::is_regular_file(it->status()))
			{
				const std::string extension = boost::to_lower_copy(it->path().extension());
				if(extension == ".data")
				{
					if(it->path().stem()[0] != '-')
						acceptor(it->path().stem());
				}
			}
		}
	}
}
