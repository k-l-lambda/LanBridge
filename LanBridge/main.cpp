
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/assign.hpp>

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

			std::vector<char*> cargv = boost::assign::list_of
				(argv[0])
				("--pitcher=Memory")
				("--catcher=Memory")
				("--memory_pitcher_repository=requests")
				("--memory_catcher_repository=responses")
				;
			for(int i = 1; i < argc; ++i)
				cargv.push_back(argv[i]);

			char* sargv[] =
			{
				argv[0], "--pitcher=Memory", "--catcher=Memory",
				"--memory_pitcher_repository=responses",
				"--memory_catcher_repository=requests",
			};

			boost::thread t(boost::bind(LanBridgeClient::main, cargv.size(), &(cargv[0])));
			return LanBridgeServer::main(5, sargv);
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
