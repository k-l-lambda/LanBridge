
#ifndef	__VIDEOBRIDGEPITCHER_H__
#define	__VIDEOBRIDGEPITCHER_H__



#include "..\include\Bridge.h"
#include "Common.h"

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>


namespace VideoBridge
{
	class Pitcher
		: public IPitcher
	{
	public:
		Pitcher(size_t frame_width, size_t frame_height, VideoFormat format = VF_24bits, int frame_x = 0, int frame_y = 0);
		~Pitcher();

	private:
		virtual void setCatcher(const CatcherPtr& catcher)	{m_Catcher = catcher;};

		virtual void write(const std::string& connection_id, const char* buffer, size_t length);

	private:
		void render(int frame_x, int frame_y, size_t frame_width, size_t frame_height);

	private:
		CatcherPtr					m_Catcher;

		VideoFormat					m_VideoFormat;
		size_t						m_PixelSize;
		size_t						m_DataWidth;

		typedef	std::vector<char>						DataBuffer;
		typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;
		DataBufferPtr				m_Data;
		boost::mutex				m_DataMutex;

		boost::mutex				m_WriteMutex;

		bool						m_End;
		boost::thread				m_RenderThread;
	};
}



#endif // !defined(__VIDEOBRIDGEPITCHER_H__)
