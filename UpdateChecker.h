#pragma once

#include <string>
#include <string_view>
#include "Comet.h"

enum VersionStage : int
{
	ALPHA = 0,
	BETA,
	RELEASE
};

struct Version
{
	int major;
	int minor;
	int patch;
	VersionStage stage = RELEASE;

	auto operator<=>(const Version& other) const = default;

	static Version Parse(std::string_view str);
	const std::string ToString() const;
};

class UpdateChecker
{
public:
	enum UpdateStatus
	{
		UP_TO_DATE,
		UPDATE_AVAILABLE,
		CHECK_ERROR
	};

	struct UpdateCheckResult
	{
		UpdateStatus status;
		Version retrievedVersion;
		std::string githubUrl;
	};

	UpdateChecker() = default;
	UpdateCheckResult CheckForUpdate();
private:
	Version currVersion = Version::Parse(COMET_VERSION);
};