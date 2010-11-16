

#ifndef	__FILESYSTEMBRIDGEPITCHER_H__
#define	__FILESYSTEMBRIDGEPITCHER_H__



#include "..\include\Bridge.h"


namespace FileSystemBridge
{
	class Pitcher
		: public IPitcher
	{
	public:
		explicit Pitcher(const std::string& station);

	private:
		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		std::string		m_Station;
	};
}



#endif // !defined(__FILESYSTEMBRIDGEPITCHER_H__)
