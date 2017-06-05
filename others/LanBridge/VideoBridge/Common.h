
#ifndef	__VIDEOBRIDGE_COMMON_H__
#define	__VIDEOBRIDGE_COMMON_H__



namespace VideoBridge
{
	typedef	std::vector<char>						DataBuffer;
	typedef	boost::shared_ptr<DataBuffer>			DataBufferPtr;


	static const std::string begin_mark	= "***BEGIN VIDEO BRIDGE REGION**";
	static const std::string end_mark	= "END VIDEO BRIDGE REGION*";

	enum VideoFormat
	{
		VF_16bits,
		VF_16bits_i16,
		VF_16bits_i8,
		VF_24bits,
	};


	inline size_t alignSize(size_t source, size_t width)
	{
		return ((source + width - 1) / width) * width;
	}

	inline std::string getLine(const char* data, size_t length)
	{
		const size_t linelen = size_t(std::find(data, data + length, '\n') - data);

		return std::string(data, linelen);
	}
}



#endif // !defined(__VIDEOBRIDGE_COMMON_H__)
