#include "Comet.h"
#include "App/MainWindow.h"
#include <string>
#include <curl/curl.h>
#include <filesystem>
#include "Utils.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>   // CommandLineToArgvW (link Shell32.lib, MSVC auto-links it via pragma below)
#pragma comment(lib, "Shell32.lib")
#endif

// returns the file passed on the command line (absolute, UTF-8), or "" if none
static std::string GetLaunchFile()
{
	std::filesystem::path p;

#ifdef _WIN32
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv)
	{
		if (argc >= 2) p = argv[1];
		LocalFree(argv);
	}
#endif

	if (p.empty()) return "";

	std::error_code ec;
	p = std::filesystem::absolute(p, ec);
	if (ec || !std::filesystem::is_regular_file(p, ec))
	{
		std::cerr << "Launch file not usable: " << p.string() << std::endl;
		return "";
	}

	auto u8 = p.u8string();
	return std::string(u8.begin(), u8.end());
}

int main()
{
	const std::string launchFile = GetLaunchFile();

	// find where Comet is and set the current dir to that so it can find the assets even if it was launched from somewhere else
	std::filesystem::path exeDir = Utils::GetExecutableDirectory();
	if (!exeDir.empty())
	{
		std::error_code ec;
		std::filesystem::current_path(exeDir, ec);
		if (ec) std::cerr << "Failed to set working directory: " << ec.message() << std::endl;
	}

	CURLcode curlResult = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (curlResult != CURLE_OK)
	{
		std::cerr << "Failed to initialize libcurl: "
			<< curl_easy_strerror(curlResult) << std::endl;

		return 1;
	}

	curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);

	std::cout << "libcurl: " << info->version << '\n';
	std::cout << "SSL: "
		<< (info->ssl_version ? info->ssl_version : "none")
		<< '\n';

	std::string cometVersion = COMET_VERSION;
	std::string versionSuffix = "";
	
	if (!cometVersion.empty() && cometVersion.back() == 'b')
	{
		cometVersion.pop_back();
		versionSuffix = "-Beta";
	}

	MainWindow mainWindow((std::string("Comet v")+cometVersion+versionSuffix).c_str());
	if (!mainWindow.IsGLReady())
	{
		curl_global_cleanup();
		return 1;
	}
	mainWindow.SetLaunchOptions(WindowLaunchOptions{ .midi = launchFile });
	mainWindow.Run();

	curl_global_cleanup();

	return 0;
}