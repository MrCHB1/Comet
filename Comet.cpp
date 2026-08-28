#include "Comet.h"
#include "App/MainWindow.h"
#include <string>
#include <curl/curl.h>

int main()
{
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
	mainWindow.Run();

	curl_global_cleanup();

	return 0;
}