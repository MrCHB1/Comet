#pragma once

#include <memory>
#include <glm/vec2.hpp>

namespace OverlayUtils
{
	void RightAlignedTableText(const char* text);

	template <size_t N, typename... Args>
	void FormatText(char(&buf)[N],
		const char* format, Args&&... args)
	{
		std::snprintf(
			buf,
			N,
			format,
			std::forward<Args>(args)...
		);
	}
}

class MIDIApp;

class AbstractOverlayRenderer
{
public:
	AbstractOverlayRenderer(MIDIApp* app) : app(app) {}
	virtual ~AbstractOverlayRenderer() = default;

	virtual bool IsShown() = 0;

	// we use heightOffset here because of the nagivation bar. when rendering a video, the navigation bar is hidden, so the counter should be rendered higher up to compensate for that. when not rendering a video, the navigation bar is visible, so the counter should be rendered lower down to avoid overlapping with it.
	virtual void Render(float heightOffset) = 0;
	virtual void OnResize(int width, int height) = 0;

	virtual glm::vec2 GetOverlayPosition() const = 0;
	virtual glm::vec2 GetOverlaySize() const = 0;
protected:
	MIDIApp* app;
};