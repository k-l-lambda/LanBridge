
#include <string>

#include <boost/program_options.hpp>

#include <windows.h>


int main(int argc, char* argv[])
{
	namespace po = boost::program_options;

	po::options_description desc("Allowed options");
	desc.add_options()
		("hwnd",		po::value<unsigned long>())
		("title",		po::value<std::string>())
		("x",			po::value<int>())
		("y",			po::value<int>())
		("width",		po::value<int>())
		("height",		po::value<int>())
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	::HWND hwnd = vm.count("hwnd") ? ::HWND(vm["hwnd"].as<unsigned long>()) : ::FindWindow(NULL, vm["title"].as<std::string>().data());
	if(hwnd)
	{
		const int x = vm.count("x") ? vm["x"].as<int>() : 0;
		const int y = vm.count("y") ? vm["y"].as<int>() : 0;
		const int width = vm.count("width") ? vm["width"].as<int>() : 171;
		const int height = vm.count("height") ? vm["height"].as<int>() : 237;

		::SetForegroundWindow(hwnd);
		long style = ::GetWindowLong(hwnd, GWL_STYLE);
		int succeed = ::SetWindowLong(hwnd, GWL_STYLE, style | WS_EX_TOPMOST);
		succeed = ::SetWindowPos(hwnd, HWND_TOPMOST, x, y, width, height, SWP_SHOWWINDOW);
		::SetForegroundWindow(hwnd);

		return 0;
	}

	return -1;
}
