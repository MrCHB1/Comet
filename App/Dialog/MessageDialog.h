#pragma once
#include "App/UI/Dialog.h"
#include <string>
#include <functional>

class MessageDialog : public Dialog
{
public:
	enum class ButtonType
	{
		OK,
		YesNo
	};

	enum class Result
	{
		OK,
		Yes,
		No
	};

	MessageDialog() : Dialog("message_dialog") {}

	const char* GetTitle() override
	{
		return title.c_str();
	}

	void DrawContent() override;

	// custom open method to configure content dynamically
	void Open(const std::string& newTitle, const std::string& newMessage, ButtonType type = ButtonType::OK, std::function<void(Result)> callback = nullptr)
	{
		title = newTitle;
		message = newMessage;
		buttonType = type;
		onResult = callback;
		openRequest = true;
	}

private:
	std::string title = "Message";
	std::string message = "";
	ButtonType buttonType = ButtonType::OK;
	std::function<void(Result)> onResult = nullptr;
};