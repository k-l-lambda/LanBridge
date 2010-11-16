
#define	_WIN32_WINNT	0x0501	// Windows XP at lowest

#include "VideoBridgeCatcher.h"
#include "Common.h"

#include <windows.h>


namespace VideoBridge
{
	std::string getLine(const char* data, size_t length)
	{
		const size_t linelen = size_t(std::find(data, data + length, '\n') - data);

		return std::string(data, linelen);
	}

	::RECT getDesktopRect()
	{
		::HWND hwnd = ::GetDesktopWindow();
		::RECT rc;
		::GetWindowRect(hwnd, &rc);

		return rc;
	}

	static const ::RECT s_DesktopRect = getDesktopRect();

	int getRectWidth(const ::RECT& rc)
	{
		return rc.right - rc.left;
	}

	int getRectHeight(const ::RECT& rc)
	{
		return rc.bottom - rc.top;
	}


	DataBufferPtr decodeVideoData(const DataBufferPtr& data, size_t pixelsize, size_t linewidth)
	{
		// skip the line of begin mark
		const char* p = &(data->front()) + linewidth;

		// read bits availability test
		const ::DWORD bits = *(::DWORD*)p;
		p += sizeof(::DWORD);

		if(bits != ~0u)
		{
			if(pixelsize != 2)
				throw std::runtime_error(__FUNCTION__": unsupported format");

			DataBufferPtr source(new DataBuffer);
			source->reserve(data->size() - linewidth - sizeof(::DWORD));

			struct _l
			{
				static void decodeLine(::WORD* source, ::WORD* data, ::DWORD bits)
				{
					std::memcpy(source, data, 30);

					::WORD compensation = data[15];
					if(bits == 0xff7fff7f)
						compensation = ((compensation >> 1) & 0xff80) | (compensation & 0x007f);

					for(size_t bit = 0; bit < 15; ++bit)
					{
						if(compensation & (1 << bit))
						{
							switch(bits)
							{
							case 0x7fff7fff:
								source[bit] |= 1 << 15;

								break;
							case 0xff7fff7f:
								source[bit] |= 1 << 7;

								break;
							default:
								throw std::runtime_error(__FUNCTION__": unsupported bits format");
							}
						}
					}
				};
			};

			// read the first segment
			source->resize(30);
			::WORD* ps = (::WORD*)&(source->front());
			_l::decodeLine(ps, (::WORD*)p, bits);

			const unsigned short datalen = *(unsigned short*)(ps + 2);
			if(datalen > data->size())
				throw std::runtime_error(__FUNCTION__": bad data length.");
			//ps += 15;
			p += 32;
			const size_t seg_count = datalen / 30 + 1;
			source->resize((seg_count + 1) * 30);
			ps = (::WORD*)&(source->front()) + 15;
			for(size_t seg = 0; seg < seg_count; ++seg)
			{
				_l::decodeLine(ps, (::WORD*)p, bits);

				ps += 15;
				p += 32;
			}

			return source;
		}
		else
			return DataBufferPtr(new DataBuffer(p, p + data->size() - linewidth - sizeof(::DWORD)));
	}


	Catcher::Catcher(size_t packsize, unsigned long interval, VideoFormat format)
		: m_PackSize(packsize)
		, m_Interval(interval)
		, m_PixelSize(format == VF_24bits ? 3 : 2)
		, m_LastFrameId(0)
		, m_End(false)
#pragma warning(suppress:4335)
		, m_CaptureThread(boost::bind(&Catcher::capture, this))
	{
	}

	Catcher::~Catcher()
	{
		m_End = true;

		m_CaptureThread.join();
	}

	size_t Catcher::read(const std::string& connection_id, char* buffer)
	{
		for(;;)
		{
			{
				boost::scoped_ptr<boost::mutex::scoped_lock> lock(new boost::mutex::scoped_lock(m_DataMutex));

				DataQueue& queue = m_ConnectionDataMap[connection_id];
				if(!queue.empty())
				{
					DataBufferPtr data = queue.front();
					assert(data);
					if(!data->empty())
						std::memcpy(buffer, &(data->front()), data->size());

					queue.pop_front();

					lock.reset();

					// send confirm response
					{
						assert(m_Pitcher);

						static const std::string s_Message = "OK";

						m_Pitcher->write("-" + connection_id + ".VideoBridge", s_Message.data(), s_Message.length());

						//std::cout << "[" << connection_id << "]	a video frame arrived, response sent." << std::endl;
					}

					return data->size();
				}
			}

			::Sleep(m_Interval);
		}

		// never get here
		assert(false);
	}

	void Catcher::acceptConnection(const AcceptorFunctor& acceptor)
	{
		boost::mutex::scoped_lock lock(m_NewConnectionsMutex);

		std::for_each(m_NewConnections.begin(), m_NewConnections.end(), acceptor);

		m_NewConnections.clear();
	}

	void Catcher::capture()
	{
		try
		{
			while(!m_End)
			{
				DataBufferPtr rawdata = captureOneFrame();
				if(rawdata)
				{
					const char* rawpointer = &(rawdata->front());
					const std::string header = getLine(rawpointer, rawdata->size());
					const std::string& connection_id = header;
					const char* body = rawpointer + header.length() + 1;
					size_t body_length = rawdata->size() - (header.length() + 1);

					DataBufferPtr buffer(body_length ? new DataBuffer(body, body + body_length) : new DataBuffer);
					assert(buffer);

					{
						boost::mutex::scoped_lock lock(m_DataMutex);

						assert(buffer);
						assert(buffer.get());
						m_ConnectionDataMap[connection_id].push_back(buffer);
					}

					if(connection_id[0] != '-')
					{
						boost::mutex::scoped_lock lock(m_NewConnectionsMutex);

						m_NewConnections.insert(connection_id);
					}
				}

				::Sleep(m_Interval);
			}
		}
		catch(const std::exception& e)
		{
			std::cerr << "Exception: " << e.what() << std::endl;
		}
	}

	namespace
	{
		static bool SaveBitmapToFile(HBITMAP hBitmap , LPSTR lpFileName)
		{
			HDC hDC;      //设备描述句柄
			int iBits;    //当前显示分辨率下每个像素所占字节数

			WORD wBitCount;   //位图中每个像素所占字节数

			//定义调色板大小， 位图中像素字节大小 ，
			//位图文件大小
			DWORD dwPaletteSize=0,
				dwBmBitsSize,
				dwDIBSize, dwWritten;

			BITMAP   Bitmap;  //位图属性结构
			BITMAPFILEHEADER    bmfHdr;  //位图文件头结构

			//位图信息头结构
			BITMAPINFOHEADER    bi;

			//指向位图信息头结构
			LPBITMAPINFOHEADER lpbi;

			//定义文件，分配内存句柄，调色板句眮E
			HANDLE   fh, hDib;
			HPALETTE hPal,hOldPal=NULL;

			//计算位图文件每个像素所占字节数
			hDC = CreateDC("DISPLAY",NULL,NULL,NULL);
			iBits = GetDeviceCaps(hDC, BITSPIXEL) * GetDeviceCaps(hDC, PLANES);
			DeleteDC(hDC);

			if (iBits <= 1)
				wBitCount = 1;
			else if (iBits <= 4)
				wBitCount = 4;
			else if (iBits <= 8)
				wBitCount = 8;
			else if (iBits <= 24)
				wBitCount = 24;
			else wBitCount = 24;

			//计算调色板大小
			if (wBitCount <= 8)
				dwPaletteSize = (1 << wBitCount) *sizeof(RGBQUAD);

			//设置位图信息头结构
			GetObject(hBitmap, sizeof(BITMAP), (LPSTR)&Bitmap);
			bi.biSize            = sizeof(BITMAPINFOHEADER);
			bi.biWidth           = Bitmap.bmWidth;
			bi.biHeight          = Bitmap.bmHeight;
			bi.biPlanes          = 1;
			bi.biBitCount         = wBitCount;
			bi.biCompression      = BI_RGB;
			bi.biSizeImage        = 0;
			bi.biXPelsPerMeter     = 0;
			bi.biYPelsPerMeter     = 0;
			bi.biClrUsed         = 0;
			bi.biClrImportant      = 0;

			dwBmBitsSize = ((Bitmap.bmWidth *wBitCount+31)/32)* 4*Bitmap.bmHeight ;  
			//为位图内容分配内磥E  
			hDib  = GlobalAlloc(GHND,dwBmBitsSize+dwPaletteSize+sizeof(BITMAPINFOHEADER));  
			lpbi = (LPBITMAPINFOHEADER)GlobalLock(hDib);  
			*lpbi = bi;  

			hPal = (HPALETTE)GetStockObject(DEFAULT_PALETTE);  
			if (hPal)  
			{  
				hDC  = ::GetDC(NULL);  
				hOldPal = SelectPalette(hDC, hPal, FALSE);  
				RealizePalette(hDC);  
			}  

			// 获取该调色板下新的像素值  
			GetDIBits(hDC, hBitmap, 0, (UINT)Bitmap.bmHeight,(LPSTR)lpbi + sizeof(BITMAPINFOHEADER)+dwPaletteSize,(LPBITMAPINFO)lpbi, DIB_RGB_COLORS);  

			//恢复调色皝E    
			if (hOldPal)  
			{  
				SelectPalette(hDC, hOldPal, TRUE);  
				RealizePalette(hDC);  
				::ReleaseDC(NULL, hDC);  
			}  

			//创建位图文件      
			fh = CreateFile(lpFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,  
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);  

			if (fh == INVALID_HANDLE_VALUE)  
				return false;  

			//设置位图文件头  
			bmfHdr.bfType = 0x4D42;  // "BM"  
			dwDIBSize    = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)  
				+ dwPaletteSize + dwBmBitsSize;    
			bmfHdr.bfSize = dwDIBSize;  
			bmfHdr.bfReserved1 = 0;  
			bmfHdr.bfReserved2 = 0;  
			bmfHdr.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER)   
				+ (DWORD)sizeof(BITMAPINFOHEADER)+ dwPaletteSize;  

			WriteFile(fh, (LPSTR)&bmfHdr, sizeof(BITMAPFILEHEADER), &dwWritten, NULL);

			WriteFile(fh, (LPSTR)lpbi, dwDIBSize, &dwWritten, NULL);

			//清除
			GlobalUnlock(hDib);
			GlobalFree(hDib);
			CloseHandle(fh);

			return true;
		}


		Catcher::DataBufferPtr snapshot(size_t pixelsize, const boost::optional<::RECT>& region = s_DesktopRect)
		{
			::HWND hwnd = ::GetDesktopWindow();
			const ::RECT rc = region ? *region : s_DesktopRect;

			const int width = rc.right - rc.left, height = rc.bottom - rc.top;

			::HDC hdc = ::GetWindowDC(hwnd);
			::HDC hmemDC = ::CreateCompatibleDC(hdc);
			::HBITMAP hbmp = ::CreateCompatibleBitmap(hdc, width, height);
			if(!hbmp)
			{
				std::cerr << __FUNCTION__": CreateCompatibleBitmap failed, snapshot failed.";
				return Catcher::DataBufferPtr(new Catcher::DataBuffer);
			}
			::HBITMAP oldbitmap = (::HBITMAP)::SelectObject(hmemDC, hbmp);
			::BitBlt(hmemDC, -rc.left, -rc.top, rc.right, rc.bottom, hdc, 0, 0, SRCCOPY);
			hbmp = (::HBITMAP)::SelectObject(hmemDC, oldbitmap);
			assert(hbmp);

			::BITMAP bitmap;
			::GetObject(hbmp, sizeof(BITMAP), &bitmap);

			Catcher::DataBufferPtr buffer(new Catcher::DataBuffer(bitmap.bmWidth * bitmap.bmHeight * pixelsize, 0i8));

			::BITMAPINFO info;
			info.bmiHeader.biSize				= sizeof(::BITMAPINFOHEADER);
			info.bmiHeader.biWidth				= bitmap.bmWidth;
			info.bmiHeader.biHeight				= bitmap.bmHeight;
			info.bmiHeader.biPlanes				= 1;
			info.bmiHeader.biBitCount			= pixelsize * 8;
			info.bmiHeader.biCompression		= BI_RGB;
			info.bmiHeader.biSizeImage			= 0;
			info.bmiHeader.biXPelsPerMeter		= 0;
			info.bmiHeader.biYPelsPerMeter		= 0;
			info.bmiHeader.biClrUsed			= 0;
			info.bmiHeader.biClrImportant		= 0;
			::GetDIBits(hdc, hbmp, 0, bitmap.bmHeight, &(buffer->front()), &info, DIB_RGB_COLORS);

			//SaveBitmapToFile(hbmp, "snapshot.bmp");

			::DeleteObject(hbmp);
			::DeleteDC(hmemDC);
			::ReleaseDC(hwnd, hdc);

			return buffer;
		}

		template<size_t pixelsize>
		boost::optional<size_t> findMark(const char* buffer, size_t length, const std::string& mark)
		{
			struct _l
			{
				static bool equal(const char* s1, const char* s2, size_t len)
				{
					for(size_t i = 0; i < len; ++i)
						if(s1[i] != s2[i])
							return false;

					return true;
				};
			};

			const std::size_t marklen = mark.length();
			const char* markdata = mark.data();
			for(const char* p = buffer + ((length - marklen) / pixelsize) * pixelsize; p >= buffer; p -= pixelsize)
			{
				//if(std::string(p, marklen) == mark)
				if(_l::equal(p, markdata, marklen))
					return p - buffer;
			}

			return boost::optional<size_t>();
		}

		template<size_t pixelsize>
		boost::optional<::RECT> findDataRegion(const Catcher::DataBufferPtr& buffer, const ::RECT& sourceRc = s_DesktopRect)
		{
			boost::optional<size_t> end = findMark<pixelsize>(&(buffer->front()), buffer->size(), end_mark);
			if(end)
			{
				boost::optional<size_t> begin = findMark<pixelsize>(&(buffer->front()), buffer->size(), begin_mark);
				if(begin)
				{
					assert(*end % pixelsize == 0);
					assert(*begin % pixelsize == 0);
					const size_t endpos = (*end + end_mark.length() - 1) / pixelsize;
					const size_t beginpos = *begin / pixelsize;

					const int width = getRectWidth(sourceRc);
					const int height = getRectHeight(sourceRc);

					::RECT rc;
					rc.left = beginpos % width;
					rc.bottom = height - beginpos / width;
					rc.right = endpos % width + 1;
					rc.top = height - (endpos / width + 1);

					return rc;
				}
			}

			return boost::optional<::RECT>();
		}
	}

	Catcher::DataBufferPtr Catcher::captureOneFrame()
	{
		boost::optional<::RECT> (*const fnFindDataRegion)(const Catcher::DataBufferPtr&, const ::RECT&) = m_PixelSize == 2 ? &findDataRegion<2> : &findDataRegion<3>;

		if(!m_DataRegion)
		{
			m_DataRegion = fnFindDataRegion(snapshot(m_PixelSize), s_DesktopRect);

			if(m_DataRegion)
				std::cout << "data region rect found: (" << m_DataRegion->left << ", " << m_DataRegion->top << ")-(" << m_DataRegion->right << ", " << m_DataRegion->bottom << ")." << std::endl;
		}

		if(m_DataRegion)
		{
			DataBufferPtr shot = snapshot(m_PixelSize, m_DataRegion);
			if(boost::optional<::RECT> subregion = fnFindDataRegion(shot, *m_DataRegion))
			{
				if(getRectWidth(*subregion) != getRectWidth(*m_DataRegion) || getRectHeight(*subregion) != getRectHeight(*m_DataRegion))
				{	// correct data resion and retry
					std::cout << "sub data region dismatch: (" << subregion->left << ", " << subregion->top << ")-(" << subregion->right << ", " << subregion->bottom << "), retry capture." << std::endl;

					m_DataRegion->left += subregion->left;
					m_DataRegion->right = m_DataRegion->left + subregion->right;
					m_DataRegion->top += subregion->top;
					m_DataRegion->bottom = m_DataRegion->top + subregion->bottom;

					return captureOneFrame();
				}

				/*// skip the line of begin mark
				const char* p = &(shot->front()) + getRectWidth(*m_DataRegion) * m_PixelSize;

				// read bits availability test
				const ::DWORD bits = *(::DWORD*)p;
				p += sizeof(::DWORD);*/
				DataBufferPtr source = decodeVideoData(shot, m_PixelSize, getRectWidth(*m_DataRegion) * m_PixelSize);
				const char* p = &(source->front());

				// read frame id
				const unsigned long frameid = *(unsigned long*)p;
				p += sizeof(unsigned long);

				if(frameid != m_LastFrameId)
				{
					// read data length
					const unsigned short datalen = *(unsigned short*)p;
					p += 2;

					if(datalen > (shot->size() - (p - &(shot->front()))))
					{
						std::cerr << __FUNCTION__": bad data length: " << datalen << std::endl;
						return DataBufferPtr();
					}

					std::cout << "a new frame \"" << frameid << "\" (" << datalen << " bytes) received." << std::endl;

					DataBufferPtr data(new DataBuffer(datalen));
					if(datalen)
						std::memcpy(&(data->front()), p, datalen);

					m_LastFrameId = frameid;

					return data;
				}
			}
			else
			{
				std::cout << "data region rect lost, retry capture." << std::endl;

				// discard data region and retry
				m_DataRegion = boost::optional<::RECT>();
				return captureOneFrame();
			}
		}

		return DataBufferPtr();
	}
}
