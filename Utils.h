#pragma once

#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include <variant>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <algorithm>
#include <type_traits>
#include <chrono>
#include <filesystem>
#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include <cmath>
#if defined(__SSE4_1__) || defined(__AVX__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64)))
#include <smmintrin.h>
#define COMET_HAS_ROUNDSD 1
#endif

struct ImVec4;

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Utils
{
	enum RGBAPackFormat
	{
		RGBA,
		ARGB,
		ABGR,
		BGRA
	};

	bool IsMIDIExtension(std::string extension);
	std::string FormatFilesize(long long bytes, int decimal);
	std::string FormatDuration(double ms);
	std::string FormatDuration2(double ms);
	std::optional<double> ParseTimeString(std::string timeString);
	bool ChooseFile(std::string& outPath, const char* extension = nullptr);
	bool SaveFile(std::string& outPath, const char* extension = nullptr);
	bool EqualsIgnoreCase(const std::string& a, const std::string& b);
	std::optional<ImVec4> ParseColor(std::variant<std::string, uint32_t> strOrInt, std::optional<ImVec4> def);
	std::string EncodeColor(ImVec4 color);
	uint32_t PackRGB(float r, float g, float b);
	std::array<float, 3> UnpackRGB(uint32_t rgb);
	uint32_t PackRGBA(float r, float g, float b, float a = 1.0f, RGBAPackFormat packFormat = RGBA);
	std::array<float, 4> UnpackRGBA(uint32_t rgba, RGBAPackFormat packFormat = RGBA);
	uint32_t BlendRGBA(uint32_t c1, uint32_t c2, float fac);
	uint32_t BlendRGBFast(uint32_t c1, uint32_t c2, uint32_t fac);
	static const std::string BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789+/";
	std::string DecodeBase64(const std::string& encoded);
	std::filesystem::path GetExecutableDirectory();
	void OpenURL(const std::string& url);
	void OpenFolder(const std::string& path);
	bool FileExists(const std::string& path);
	bool FileExists(const std::wstring& path);
	bool FolderExists(const std::string& path);
	template <typename T>
	double GetCurrTime()
	{
		return (double)std::chrono::duration_cast<T>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
	template <typename T>
	std::string FormatWithCommas(T value)
	{
		static_assert(std::is_integral_v<T>, "T must be an integer");

		std::string digits = std::to_string(value);

		const bool negative = digits[0] == '-';
		const std::size_t digitCount = digits.size() - negative;
		const std::size_t commaCount = (digitCount - 1) / 3;

		std::string result;
		result.resize(digits.size() + commaCount);

		std::size_t src = digits.size();
		std::size_t dst = result.size();
		int group = 0;

		while (src > static_cast<std::size_t>(negative))
		{
			result[--dst] = digits[--src];

			if (++group == 3 && src > static_cast<std::size_t>(negative))
			{
				result[--dst] = ',';
				group = 0;
			}
		}

		if (negative)
			result[0] = '-';

		return result;
	}
	void AddFilePickerField(const char* label, std::filesystem::path& path, const char* extension, bool saving = false);
	
	YAML::Node Vec3ToNode(glm::vec3 vector);
	glm::vec3 NodeToVec3(const YAML::Node& node);
	YAML::Node Vec4ToNode(glm::vec4 vector);
	glm::vec4 NodeToVec4(const YAML::Node& node);

	std::shared_ptr<std::istream> TryGetStream(std::filesystem::path path);

	namespace Dialogs
	{
		enum DialogType
		{
			DIALOG_OK,
			DIALOG_WARNING,
			DIALOG_ERROR
		};

		static void ShowDialog(const char* title, const char* content, DialogType dialogType)
		{
#if defined(_WIN32)
			UINT uType = 0;
			switch (dialogType)
			{
			case DialogType::DIALOG_OK:
				uType = MB_ICONINFORMATION;
				break;
			case DialogType::DIALOG_ERROR:
				uType = MB_ICONERROR;
				break;
			case DialogType::DIALOG_WARNING:
				uType = MB_ICONWARNING;
				break;
			}

			MessageBoxA(nullptr, content, title, MB_OK | uType);
#else
			// no native message box on this platform, at least surface it on stderr
			std::fprintf(stderr, "[%s] %s\n", title, content);
			(void)dialogType;
#endif
		}
	}

	class ProfileBlock
	{
		using Clock = std::chrono::steady_clock;

		struct ProfileStats
		{
			size_t frames = 0;
			double totalMs = 0.0;
		};

		inline static constexpr size_t FRAMES_PER_DEBUG = 15;
		inline static std::unordered_map<std::string, ProfileStats> sections;
	public:
		ProfileBlock(const char* sectionName)
			: sectionName(sectionName), startTime(Clock::now())
		{
		}

		~ProfileBlock()
		{
			const double diffMs = std::chrono::duration<double, std::milli>(Clock::now() - startTime).count();
			auto& stats = sections[sectionName];
			stats.totalMs += diffMs;

			if (++stats.frames >= FRAMES_PER_DEBUG)
			{
				std::cout << sectionName << ": " << stats.totalMs / FRAMES_PER_DEBUG << "ms/frame\n";

				stats.frames = 0;
				stats.totalMs = 0.0;
			}
		}
	private:
		const char* sectionName = nullptr;
		Clock::time_point startTime;
	};

	// Round half to even. One instruction on x64; independent of the FP rounding mode there.
	inline double RoundEven(double v)
	{
#if defined(COMET_HAS_ROUNDSD)
		const __m128d x = _mm_set_sd(v);
		return _mm_cvtsd_f64(_mm_round_sd(x, x, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
#else
		// Portable fallback: valid for |v| < 2^51, assumes default FP mode and no fast-math.
		constexpr double M = 6755399441055744.0; // 1.5 * 2^52
		return (v + M) - M;
#endif
	}
}