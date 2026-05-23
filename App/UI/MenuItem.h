#include <string>
#include <functional>
#include <vector>
#include "imgui.h";

class MenuItem
{
public:
	const char* label = "Item";
	bool enabled = true;

	virtual void Draw() = 0;

	void SetEnabled(bool isEnabled)
	{
		enabled = isEnabled;
	}
};

class MenuButton : public MenuItem
{
public:
	MenuButton(const char* label)
		: MenuButton(label, []() {})
	{ };

	MenuButton(const char* label, std::function<void()> clickAction)
		: MenuButton(label, clickAction, true)
	{ };

	MenuButton(const char* label, std::function<void()> clickAction, bool enabled)
	{
		this->label = label;
		this->clickAction = clickAction;
		this->enabled = enabled;
	}

	void Draw() override
	{
		if (ImGui::MenuItem(label, nullptr, false, enabled))
		{
			clickAction();
		}
	}
private:
	std::function<void()> clickAction;
};

class SubMenu : public MenuItem
{
public:
	SubMenu(const char* label)
	{
		this->label = label;
	}
	void Draw() override
	{
		if (ImGui::BeginMenu(label, enabled))
		{
			for (MenuItem* item : items)
			{
				item->Draw();
			}
			ImGui::EndMenu();
		}
	}
	void AddItem(MenuItem* item)
	{
		items.push_back(item);
	}

	std::vector<MenuItem*>& GetItems()
	{
		return items;
	}
private:
	std::vector<MenuItem*> items;
};