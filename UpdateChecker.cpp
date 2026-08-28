#include "UpdateChecker.h"
#include <string_view>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

Version Version::Parse(std::string_view str)
{
	VersionStage stage = RELEASE;

	if (str.starts_with('v')) str.remove_prefix(1);
	if (str.ends_with('a'))
	{
		stage = ALPHA;
		str.remove_suffix(1);
	}
	if (str.ends_with('b'))
	{
		stage = BETA;
		str.remove_suffix(1);
	}

	Version v{};
	std::sscanf(str.data(), "%d.%d.%d", &v.major, &v.minor, &v.patch);
	v.stage = stage;

	return v;
}

UpdateChecker::UpdateCheckResult UpdateChecker::CheckForUpdate()
{
#pragma region Github version retrieval
	std::cout << "Update check begin." << std::endl;

	try
	{
		CURL* curl = curl_easy_init();
		if (!curl)
		{
			std::cout << "  Failed to initialize Curl" << std::endl;
			return { UpdateStatus::CHECK_ERROR, {} };
		}

		std::string response;

		curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/MrCHB1/Comet/releases/latest");

		curl_easy_setopt(curl, CURLOPT_USERAGENT, "Comet");
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			+[](char* ptr, size_t size, size_t nmemb, void* userdata)
			{
				auto* str = static_cast<std::string*>(userdata);
				str->append(ptr, size * nmemb);
				return size * nmemb;
			});

		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

		CURLcode result = curl_easy_perform(curl); // CRASHES HERE

		long httpCode = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
		curl_easy_cleanup(curl);

		if (result != CURLE_OK)
		{
			std::cout << "  Curl error: "
				<< curl_easy_strerror(result) << std::endl;
			return { UpdateStatus::CHECK_ERROR, {} };
		}

		if (httpCode != 200)
		{
			std::cout << "  GitHub returned HTTP " << httpCode << "." << std::endl;
			return { UpdateStatus::CHECK_ERROR, {} };
		}
#pragma endregion

		auto json = nlohmann::json::parse(response, nullptr, false);
		if (json.is_discarded())
		{
			std::cout << "  Invalid JSON retrieved." << std::endl;
			return { UpdateStatus::CHECK_ERROR, {} };
		}

		std::string latestVersionStr = json.value("tag_name", "");

		if (latestVersionStr.empty())
		{
			std::cout << "  JSON retrieved but did not have a tag." << std::endl;
			return { UpdateStatus::CHECK_ERROR, {} };
		}

		Version latestVersion = Version::Parse(latestVersionStr);

		if (latestVersion > currVersion)
		{
			std::cout << "  Update available: "
				<< latestVersionStr << std::endl;

			return { UpdateStatus::UPDATE_AVAILABLE, latestVersion };
		}

		if (latestVersion < currVersion)
		{
			std::cout << "  Latest release is behind this build." << std::endl;
		}
		else
		{
			std::cout << "  Already up to date." << std::endl;
		}

		std::cout << "  Retrieved version: " << latestVersionStr << std::endl;
		std::cout << "  Build (current) version: " << COMET_VERSION << std::endl;

		return { UpdateStatus::UP_TO_DATE, latestVersion };
	}
	catch (const std::exception& e)
	{
		std::cout << "  Exception: " << e.what() << std::endl;
		return { UpdateStatus::CHECK_ERROR, {} };
	}
	catch (...)
	{
		std::cout << "  Something went wrong while trying to check for an update." << std::endl;
		return { UpdateStatus::CHECK_ERROR, {} };
	}
}