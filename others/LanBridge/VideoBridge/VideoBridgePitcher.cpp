
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "VideoBridgePitcher.h"
#include "Common.h"

#include <windows.h>
#pragma comment(lib, "Winmm.lib")


namespace VideoBridge
{
	DataBufferPtr encodeVideoData(const DataBufferPtr& source, VideoFormat format, size_t linewidth)
	{
		switch(format)
		{
		case VF_16bits_i16:
		case VF_16bits_i8:
			{
				const size_t datalen = alignSize(sizeof(::DWORD) + (source->size() / 30) * 32, linewidth);
				DataBufferPtr data(new DataBuffer(datalen, 0i8));
				::WORD* p = (::WORD*)&(data->front());

				// write bits availability test
				*(::DWORD*)p = ~0u;
				p += 2;

				source->resize(sizeof(::DWORD) + alignSize(source->size() - sizeof(::DWORD), 30));
				const ::WORD* ps = (::WORD*)(&(source->front()) + sizeof(::DWORD));		// skip 4 bits availability test

				const size_t seg_count = (source->size() - sizeof(::DWORD)) / 30;
				for(size_t seg = 0; seg < seg_count; ++seg)
				{
					std::memcpy(p, ps, 30);

					::WORD compensation = 0;
					switch(format)
					{
					case VF_16bits_i16:
						for(size_t bit = 0; bit < 15; ++bit)
							if(ps[bit] & (1 << 15))
								compensation |= 1 << bit;

						break;
					case VF_16bits_i8:
						for(size_t bit = 0; bit < 15; ++bit)
							if(ps[bit] & (1 << 7))
								compensation |= 1 << bit;

						compensation = ((compensation << 1) & 0xff80) | (compensation & 0x007f);

						break;
					}

					p[15] = compensation;

					p += 16;
					ps += 15;
				}

				return data;
			}
		default:
			source->resize(alignSize(source->size(), linewidth));
			return source;
		}
	}


	class RenderWindow
	{
	public:
		RenderWindow(size_t width, size_t height, VideoFormat format, size_t pixelsize)
			: m_Width(width)
			, m_Height(height)
			, m_hWnd(NULL)
			, m_VideoFormat(format)
			, m_PixelSize(pixelsize)
			, m_hDc(NULL)
			, m_hBackDc(NULL)
			//, m_hBitmap(NULL)
			, m_BeginMarkBuffer(width * pixelsize, 0i8)
			, m_EndMarkBuffer(width * pixelsize, 0i8)
		{
			::WNDCLASS wndClass;
			wndClass.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
			wndClass.lpfnWndProc	= &RenderWindow::WndProc;
			wndClass.cbClsExtra		= 0;
			wndClass.cbWndExtra		= 0;
			wndClass.hInstance		= ::GetModuleHandle(NULL);
			wndClass.hIcon			= 0;
			wndClass.hCursor		= ::LoadCursor(NULL, IDC_ARROW);
			wndClass.hbrBackground	= (::HBRUSH)::GetStockObject(BLACK_BRUSH);
			wndClass.lpszMenuName	= NULL;
			wndClass.lpszClassName	= "VideoBridge";

			if(!::RegisterClass(&wndClass))
				throw std::runtime_error("RegisterClass failed");

			::DWORD style = WS_OVERLAPPEDWINDOW & (~WS_MAXIMIZEBOX) | WS_EX_TOPMOST;
				//WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_MINIMIZEBOX|WS_VISIBLE
				//(WS_OVERLAPPEDWINDOW & (~WS_MAXIMIZEBOX))

			m_hWnd = ::CreateWindow("VideoBridge", "VideoBridge", style, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				::GetDesktopWindow(), NULL, wndClass.hInstance, NULL);

			{
				::RECT rcWnd, rcClient;
				::GetWindowRect(m_hWnd, &rcWnd);
				::GetClientRect(m_hWnd, &rcClient);

				::SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0,
						m_Width + rcWnd.right - rcWnd.left - (rcClient.right - rcClient.left),
						m_Height + rcWnd.bottom - rcWnd.top - (rcClient.bottom - rcClient.top),
						SWP_SHOWWINDOW);
			}

			::SetWindowLong(m_hWnd, GWL_USERDATA, (::LONG)this);

			m_hDc = ::GetDC(m_hWnd);
			m_hBackDc = ::CreateCompatibleDC(NULL);

			//m_hBitmap = ::CreateCompatibleBitmap(m_hDc, m_Width, m_Height);

			// mark must occupy an entire pixel
			assert(begin_mark.length() % m_PixelSize == 0);
			assert(end_mark.length() % m_PixelSize == 0);
			assert(begin_mark.length() < m_BeginMarkBuffer.size());
			assert(end_mark.length() < m_EndMarkBuffer.size());
			std::memcpy(&(m_BeginMarkBuffer.front()), begin_mark.data(), begin_mark.length());
			std::memcpy(&(m_EndMarkBuffer.front()) + m_EndMarkBuffer.size() - end_mark.length(), end_mark.data(), end_mark.length());
		};

		~RenderWindow()
		{
			::DestroyWindow(m_hWnd);
			::UnregisterClass("VideoBridge", NULL);
		};

		void pumpMessages()
		{
			::MSG msg;

			while(::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
			{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
			}

			//::RedrawWindow(m_hWnd, NULL, NULL, RDW_INTERNALPAINT);
		};

		void setData(const DataBufferPtr& data)
		{
			m_Data = encodeVideoData(data, m_VideoFormat, m_Width * m_PixelSize);

			refresh();
		};

		void refresh()
		{
			//::UpdateWindow(m_hWnd);
			::RedrawWindow(m_hWnd, NULL, NULL, RDW_INTERNALPAINT);
		};

	private:
		LRESULT proc(::UINT uMsg, ::WPARAM wParam, ::LPARAM lParam)
		{
			switch(uMsg)
			{
			case WM_CLOSE:
				return 0;
			case WM_PAINT:
				paint();

				break;
			}

			return ::DefWindowProc(m_hWnd, uMsg, wParam, lParam);
		};

		static LRESULT CALLBACK WndProc(::HWND hWnd, ::UINT uMsg, ::WPARAM wParam, ::LPARAM lParam)
		{
			RenderWindow* self = (RenderWindow*)::GetWindowLong(hWnd, GWL_USERDATA);
			if(self)
				return self->proc(uMsg, wParam, lParam);

			return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
		};

		void paint()
		{
			if(m_Data)
			{
				::HBITMAP bmpSource = ::CreateCompatibleBitmap(m_hDc, m_Width, m_Height);
				{
					size_t bodylines = (m_Data->size() + (m_Width * m_PixelSize) - 1) / (m_Width * m_PixelSize);

					/*boost::shared_array<char> buffer(new char[m_Width * m_Height * 3]);
					static size_t index = 0;
					++index;
					for(int y = 0; y < m_Height; ++ y)
						for(int x = 0; x < m_Width; ++ x)
						{
							char* p = buffer.get() + (y * m_Width + x) * 3;
							p[0] = p[1] = p[2] = 0;
							p[(y + index) % 3] = x % 0x80 + 0x80;
						}*/

					::BITMAPINFO info;
					info.bmiHeader.biSize = sizeof(::BITMAPINFOHEADER);
					info.bmiHeader.biWidth = m_Width;
					info.bmiHeader.biHeight = m_Height;
					info.bmiHeader.biPlanes = 1;
					info.bmiHeader.biBitCount = m_PixelSize * 8;
					info.bmiHeader.biCompression = BI_RGB;
					info.bmiHeader.biSizeImage = m_Width * m_Height * m_PixelSize;
					info.bmiHeader.biXPelsPerMeter = 0;
					info.bmiHeader.biYPelsPerMeter = 0;
					info.bmiHeader.biClrUsed = 0;
					info.bmiHeader.biClrImportant = 0;

					::SetDIBits(m_hDc, (::HBITMAP)bmpSource, 0, 1, &(m_BeginMarkBuffer.front()), &info, DIB_RGB_COLORS);
					int donelines = ::SetDIBits(m_hDc, (::HBITMAP)bmpSource, 1, bodylines, &(m_Data->front()), &info, DIB_RGB_COLORS);
					//std::cout << "donelines: " << donelines << std::endl;
					assert(donelines);
					::SetDIBits(m_hDc, (::HBITMAP)bmpSource, m_Height - 1, 1, &(m_EndMarkBuffer.front()), &info, DIB_RGB_COLORS);
				}

				::HBITMAP bmp = ::CreateCompatibleBitmap(m_hDc, m_Width, m_Height);
				::SelectObject(m_hBackDc, bmp);
				::HDC hSourceDc = CreateCompatibleDC(NULL);
				::SelectObject(hSourceDc, bmpSource);
				::BitBlt(m_hBackDc, 0, 0, m_Width, m_Height, hSourceDc, 0, 0,SRCCOPY);
				::BitBlt(m_hDc, 0, 0, m_Width, m_Height, m_hBackDc, 0, 0, SRCCOPY);
				//::BitBlt(m_hDc, 0, 0, m_Width, m_Height, hSourceDc, 0, 0, SRCCOPY);

				::DeleteDC(hSourceDc);
				::DeleteObject(bmp);
				::DeleteObject(bmpSource);
			}
		};

	private:
		int					m_Width;
		int					m_Height;

		VideoFormat			m_VideoFormat;
		size_t				m_PixelSize;

		::HWND				m_hWnd;

		::HDC				m_hDc;
		::HDC				m_hBackDc;

		//::HBITMAP			m_hBitmap;

		DataBufferPtr		m_Data;

		DataBuffer			m_BeginMarkBuffer;
		DataBuffer			m_EndMarkBuffer;
	};


	Pitcher::Pitcher(size_t frame_width, size_t frame_height, VideoFormat format)
		: m_VideoFormat(format)
		, m_PixelSize(format == VF_24bits ? 3 : 2)
		, m_DataWidth(frame_width * m_PixelSize)
		, m_End(false)
#pragma warning(suppress:4335)
		, m_RenderThread(boost::bind(&Pitcher::render, this, frame_width, frame_height))
	{
	}

	Pitcher::~Pitcher()
	{
		m_End = true;

		m_RenderThread.join();
	}

	void Pitcher::write(const std::string& connection_id, const char* buffer, size_t length)
	{
		const std::string header = connection_id + "\n";
		const size_t datalength = header.length() + length;

		DataBufferPtr data(new DataBuffer(datalength + 0x100));
		char* p = &(data->front());

		// write bits availability test
		*(::DWORD*)p = ~0u;
		p += sizeof(::DWORD);

		// write frame id
		{
			static unsigned long s_FrameId = ::timeGetTime() % 0x1000000;
			static boost::mutex s_FrameIdMutex;

			{
				boost::mutex::scoped_lock lock(s_FrameIdMutex);

				*(unsigned long*)p = s_FrameId++;
			}
			p += sizeof(unsigned long);
		}

		// write data length
		*(unsigned short*)p = unsigned short(datalength);
		p += sizeof(unsigned short);

		std::memcpy(p, header.data(), header.length());
		p += header.length();
		if(length)
			std::memcpy(p, buffer, length);

		{
			boost::mutex::scoped_lock lock(m_WriteMutex);

			{
				boost::mutex::scoped_lock lock(m_DataMutex);

				m_Data = data;
			}

			// wait for confirm response
			{
				char buffer[0x100];
				size_t len = m_Catcher->read("-" + connection_id + ".VideoBridge", buffer);

				//std::cout << "[" << connection_id << "]	a video frame sent and arrived, response: " << std::string(buffer, len) << std::endl;
			}
		}
	}

	void Pitcher::render(size_t frame_width, size_t frame_height)
	{
		RenderWindow window(frame_width, frame_height, m_VideoFormat, m_PixelSize);

		while(!m_End)
		{
			window.pumpMessages();

			if(!m_Data)
			{
				::Sleep(20);

				window.refresh();

				continue;
			}

			// send data
			{
				assert(m_Data);
				window.setData(m_Data);
				{
					boost::mutex::scoped_lock lock(m_DataMutex);

					m_Data.reset();
				}
			}
		}
	}
}
