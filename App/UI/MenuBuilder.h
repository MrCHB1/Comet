#include "MenuItem.h"

class MenuBuilder
{
public:
	MenuBuilder() = default;
	~MenuBuilder()
	{
		for (SubMenu& menu : menus)
		{
			for (MenuItem* item : menu.GetItems())
			{
				delete item;
			}
		}
		menus.clear();
	}
	SubMenu& CreateMenu(std::string label)
	{
		menus.emplace_back(label);
		return menus.back();
	}
	void Draw()
	{
		if (ImGui::BeginMainMenuBar())
		{
			for (SubMenu& menu : menus)
			{
				menu.Draw();
			}
			ImGui::EndMainMenuBar();
		}
	}
private:
	std::vector<SubMenu> menus = { };
};