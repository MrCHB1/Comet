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
	virtual ImGuiWindowFlags GetWindowFlags() { return ImGuiWindowFlags_AlwaysAutoResize; }
	virtual ImVec2 GetInitialSize() { return ImVec2(0, 0); }
	virtual void OnOpen() {}

	Dialog(const std::string& dlgId) : dlgId(dlgId) {}
	virtual ~Dialog() = default;

	void DrawDialog()
	{
		auto title = GetTitle();

		if (openRequest)
		{
			ImGui::OpenPopup(title);
			openRequest = false;
			OnOpen();
		}

		auto windowFlags = GetWindowFlags();
		if (windowFlags & ImGuiWindowFlags_AlwaysAutoResize)
		{
			ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
			ImGui::SetNextWindowContentSize(ImVec2(0, 0));
			ImGui::SetNextWindowSizeConstraints(ImVec2(300, 100), ImVec2(FLT_MAX, FLT_MAX));
			ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		}
		else
		{
			ImGui::SetWindowSize(GetInitialSize(), ImGuiCond_Once);
		}

		if (ImGui::BeginPopupModal(title, NULL, windowFlags))
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

	bool IsOpened()
	{
		auto title = GetTitle();
		if (title == nullptr || title[0] == '\0') return false;
		return ImGui::IsPopupOpen(title);
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

	bool IsAnyDialogOpened()
	{
		for (const auto& [_, dialog] : dialogRegistry)
		{
			if (dialog->IsOpened()) return true;
		}
		return false;
	}
private:
	std::unordered_map<std::string, std::unique_ptr<Dialog>> dialogRegistry;
};