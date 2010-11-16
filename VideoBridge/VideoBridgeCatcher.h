
#ifndef	__VIDEOBRIDGECATCHER_H__
#define	__VIDEOBRIDGECATCHER_H__



#include "..\include\Bridge.h"
#include "Common.h"

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>

#include <windows.h>


namespace VideoBridge
{
	class Catcher
		: public ICatcher
	{
	public:
		typedef	std::vector<char>						DataBuffer;
		typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;
		typedef	std::deque<DataBufferPtr>				DataQueue;
		typedef	std::map<std::string, DataQueue>		ConnectionDataMap;

	public:
		Catcher(size_t packsize, unsigned long interval, VideoFormat format = VF_24bits);
		~Catcher();

	private:
		virtual void setPitcher(const PitcherPtr& pitcher)	{m_Pitcher = pitcher;};

		virtual size_t read(const std::string& connection_id, char* buffer);

		virtual void acceptConnection(const AcceptorFunctor& acceptor);

	private:
		void capture();

		DataBufferPtr captureOneFrame();

	private:
		PitcherPtr					m_Pitcher;

		size_t						m_PackSize;
		unsigned long				m_Interval;
		size_t						m_PixelSize;

		ConnectionDataMap			m_ConnectionDataMap;
		boost::mutex				m_DataMutex;

		std::set<std::string>		m_NewConnections;
		boost::mutex				m_NewConnectionsMutex;

		unsigned long				m_LastFrameId;

		bool						m_End;
		boost::thread				m_CaptureThread;

		boost::optional<::RECT>		m_DataRegion;
	};
}



#endif // !defined(__VIDEOBRIDGECATCHER_H__)
