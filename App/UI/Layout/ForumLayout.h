#pragma once

#include <utility>
#include <string>

namespace Comet::UI::Layout
{
	class ForumLayoutBreak;

	class ForumLayout
	{
	public:
		ForumLayout(const char* id, float labelWidth = 180.0f);
		~ForumLayout();

		ForumLayout(const ForumLayout&) = delete;
		ForumLayout& operator=(const ForumLayout&) = delete;

		template <typename F>
		void AddField(const char* id, const char* label, F&& contentFn)
		{
			if (!hasTable) BeginTable();
			if (!hasTable) return;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label);

			ImGui::TableSetColumnIndex(1);
			ImGui::PushID(id);
			std::forward<F>(contentFn)();
			ImGui::PopID();
		}

		template <typename F>
		void AddFieldWithTooltip(const char* id, const char* label, const char* tooltip, F&& contentFn)
		{
			if (!hasTable) BeginTable();
			if (!hasTable) return;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextWrapped(label);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(tooltip);

			ImGui::TableSetColumnIndex(1);
			ImGui::PushID(id);
			std::forward<F>(contentFn)();
			ImGui::PopID();
		}

		void AddHeader(const char* text, float size = 1.2f);
		void AddLargeHeader(const char* text);
	private:
		bool hasTable = false;
		const char* tableId = nullptr;
		float labelWidth = 180.0f;
		size_t tableNum = 0;

		void BeginTable();

		friend class ForumLayoutBreak;
	};

	class ForumLayoutBreak
	{
	public:
		ForumLayoutBreak(ForumLayout* layout) : layout(layout)
		{
			if (layout->hasTable)
			{
				ImGui::EndTable();
			}
		}
	
		~ForumLayoutBreak()
		{
			if (layout->hasTable) layout->BeginTable();
		}
	private:
		ForumLayout* layout;
	};

	bool SliderFloatField(ForumLayout& layout, const char* id, const char* label,
		const char* tooltip, float& target, float lo, float hi);

	bool SliderIntField(ForumLayout& layout, const char* id, const char* label,
		const char* tooltip, int& target, int lo, int hi);
}