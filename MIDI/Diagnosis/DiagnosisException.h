#pragma once

#include <exception>
#include <string>
#include <vector>
#include <memory>

class DiagnosisException : public std::exception
{
	using Base = std::exception;
	using Args = std::vector<std::string>;
public:
	
	DiagnosisException(std::string msg, Args msgFormat = {})
		: message(std::move(msg)), msgFormat(msgFormat) { }

	DiagnosisException(std::string msg, std::exception_ptr cause, Args msgFormat = {})
		: message(std::move(msg)), cause(std::move(cause)), msgFormat(std::move(msgFormat)) { }

	const char* what() const noexcept override
	{
		return message.c_str();
	}
private:
	std::string message;
	std::exception_ptr cause;
	Args msgFormat;
};