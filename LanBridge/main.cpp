
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "..\include\Log.h"


namespace LanBridgeClient
{
	int main(int argc, char* argv[]);
}

namespace LanBridgeServer
{
	int main(int argc, char* argv[]);
}


int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("Allowed options");
	desc.add_options()
		("usage",		po::value<std::string>())
	;

	try
	{
		po::variables_map vm;
		po::store(po::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
		po::notify(vm);

		const std::string usage = vm.count("usage") ? vm["usage"].as<std::string>() : "station";

		if(usage == "client")
		{
			Log::shell(Log::Msg_Information) << "LanBridge Client starting...";

			return LanBridgeClient::main(argc, argv);
		}
		else if(usage == "server")
		{
			Log::shell(Log::Msg_Information) << "LanBridge Server starting...";

			return LanBridgeServer::main(argc, argv);
		}
		else if(usage == "station")
		{
			Log::shell(Log::Msg_Information) << "LanBridge Station starting...";

			const int argcount = argc + 2;
			std::vector<char*> argvalues;
			for(size_t i = 0; i < argc; ++i)
				argvalues.push_back(argv[i]);
			argvalues.push_back("--pitcher=Memory");
			argvalues.push_back("--catcher=Memory");

			boost::thread t(boost::bind(LanBridgeClient::main, argcount, &(argvalues[0])));
			return LanBridgeServer::main(argcount, &(argvalues[0]));
		}
		else
			throw std::runtime_error("unknown usage: " + usage);
	}
	catch(const std::exception& e)
	{
		Log::shell(Log::Msg_Fatal) << "Exception: " << e.what();
	}

	std::system("pause");

	return 0;
}
