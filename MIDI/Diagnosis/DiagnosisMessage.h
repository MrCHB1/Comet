#pragma once

#include <string>
#include <tuple>

struct DiagnosisMessage
{
	std::string name;
	std::tuple<std::string> descFormat;

	template <typename... Args>
	DiagnosisMessage(std::string name, Args&&... descFormat)
		: name(name)
	{
		this->descFormat = std::make_tuple(std::forward<Args>(descFormat)...);
	}
};