#include <string>
#include <functional>
#include <vector>
#include "imgui.h"

class MenuItem
{
public:
	std::string label = "Item";
	bool enabled = true;

	virtual void Draw() = 0;
	virtual ~MenuItem() = default;

	void SetEnabled(bool isEnabled)
	{
		enabled = isEnabled;
	}
};

class MenuButton : public MenuItem
{
public:
	MenuButton(std::string label)
		: MenuButton(label, []() {})
	{ };

	MenuButton(std::string label, std::function<void()> clickAction)
		: MenuButton(label, clickAction, true)
	{ };

	MenuButton(std::string label, std::function<void()> clickAction, bool enabled)
	{
		this->label = label;
		this->clickAction = clickAction;
		this->enabled = enabled;
	}

	void Draw() override
	{
		if (ImGui::MenuItem(label.c_str(), nullptr, false, enabled))
		{
			clickAction();
		}
	}
private:
	std::function<void()> clickAction;
};

class MenuCheckbox : public MenuItem
{
public:
	MenuCheckbox(std::string label, bool* valuePtr)
	{
		this->label = label;
		this->valuePtr = valuePtr;
	}

	void Draw() override
	{
		ImGui::MenuItem(label.c_str(), nullptr, valuePtr, enabled);
	}
private:
	bool* valuePtr;
};

class SubMenu : public MenuItem
{
public:
	SubMenu(std::string label)
	{
		this->label = label;
	}
	void Draw() override
	{
		if (ImGui::BeginMenu(label.c_str(), enabled))
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