#pragma once

#include <functional>
#include <vector>

class Progress
{
public:
	Progress(const char* name) : name(name)
	{
		bars = { };
	}

	const char* GetName() const
	{
		return name;
	}

	int GetBarCount()
	{
		return bars.size();
	}

	std::function<double()> GetBar(int index)
	{
		return bars[index];
	}

	void Stop() {};
protected:
	void SetName(const char* name)
	{
		this->name = name;
	}

	void AddBar(std::function<double()> bar)
	{
		bars.push_back(bar);
	}

	bool RemoveBar(std::function<double()> bar)
	{
		for (int i = 0; i < bars.size(); i++)
		{
			if (bars[i].target_type() == bar.target_type())
			{
				bars.erase(bars.begin() + i);
				return true;
			}
		}
		return false;
	}

	void ClearBars()
	{
		bars.clear();
	}
private:
	const char* name;
	std::vector<std::function<double()>> bars;
};