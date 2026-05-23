#pragma once

#include "../Utils.h"

inline static std::string KEY_STR = Utils::DecodeBase64("MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEbLRXEO3p09xUAgYZT2zwBkT4pwhm0TZHJvXrbDnWSNNGseWenkb3rec6fxKcVfttUhMppjtgAuS/NpNzmd2i4g==");

class ResourcePackVerifier
{
private:
	const char* KEY_BYTES = KEY_STR.c_str();
};