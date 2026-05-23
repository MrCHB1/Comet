#pragma once

#include "imgui.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <type_traits>

class Dialog
{
public:
	virtual const char* GetTitle() = 0;
	virtual void DrawContent() = 0;

	Dialog(const std::string& dlgId) : dlgId(dlgId) {}
	virtual ~Dialog() = default;

	void DrawDialog()
	{
		auto title = GetTitle();

		if (openRequest)
		{
			ImGui::OpenPopup(title);
			openRequest = false;
		}

		if (ImGui::BeginPopupModal(title, NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			DrawContent();
			ImGui::EndPopup();
		}
	}

	template <typename... Args>
	void Open(Args&&... args)
	{
		// ImGui::OpenPopup(dlgId.c_str());
		openRequest = true;
	}

	void Close()
	{
		ImGui::CloseCurrentPopup();
	}

	const std::string& GetID() const
	{
		return dlgId;
	}
private:
	bool openRequest = false;
	const std::string dlgId;
};

class DialogManager
{
public:
	DialogManager() = default;

	template <typename T, typename... Args>
	void RegisterDialog(Args&&... args)
	{
		static_assert(std::is_base_of_v<Dialog, T>);

		auto dialog = std::make_unique<T>(std::forward<Args>(args)...);
		dialogRegistry[dialog->GetID()] = std::move(dialog);
	}
	
	template <typename T>
	T* GetDialog()
	{
		static_assert(std::is_base_of_v<Dialog, T>);
		for (auto& [_, dlg] : dialogRegistry)
		{
			if (auto casted = dynamic_cast<T*>(dlg.get()))
				return casted;
		}

		return nullptr;
	}

	void DrawDialogs()
	{
		for (const auto& [_, dialog] : dialogRegistry)
		{
			dialog->DrawDialog();
		}
	}
private:
	std::unordered_map<std::string, std::unique_ptr<Dialog>> dialogRegistry;
};