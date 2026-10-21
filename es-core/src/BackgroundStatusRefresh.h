#pragma once

#include <cstdint>
#include <vector>

class GuiComponent;

template <typename Component, typename VisitStatus, typename GetChildren, typename RefreshChild>
bool refreshBackgroundStatusTree(Component* component, VisitStatus visitStatus, GetChildren getChildren, RefreshChild refreshChild)
{
	bool changed = visitStatus(component);
	for (auto child : getChildren(component))
		changed = refreshChild(child) || changed;

	return changed;
}

// Tracks awake menu coverage independently of animation delta time.
class BackgroundStatusRefresh
{
public:
	static constexpr uint32_t INTERVAL_MS = 100;

	BackgroundStatusRefresh();

	// Returns true at most once per observation when a status pass is due.
	bool observe(bool eligible, const void* rootIdentity, uint32_t now);

	// Returns -1 while inactive, otherwise the remaining time to the next pass.
	int getTimeout(uint32_t now) const;
	void reset();

private:
	bool mActive;
	const void* mRootIdentity;
	uint32_t mNextDue;
};
