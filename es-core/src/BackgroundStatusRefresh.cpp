#include "BackgroundStatusRefresh.h"
#include "GuiComponent.h"

BackgroundStatusRefresh::BackgroundStatusRefresh() : mActive(false), mRootIdentity(nullptr), mNextDue(0)
{
}

bool BackgroundStatusRefresh::observe(bool eligible, const void* rootIdentity, uint32_t now)
{
	if (!eligible || rootIdentity == nullptr)
	{
		reset();
		return false;
	}

	if (!mActive || mRootIdentity != rootIdentity)
	{
		mActive = true;
		mRootIdentity = rootIdentity;
		mNextDue = now;
	}

	// Unsigned subtraction makes the deadline comparison safe across uint32_t wrap.
	const uint32_t lateBy = now - mNextDue;
	if (lateBy >= 0x80000000u)
		return false;

	// Preserve phase for normal frame jitter, but discard a missed full interval.
	mNextDue = lateBy >= INTERVAL_MS ? now + INTERVAL_MS : mNextDue + INTERVAL_MS;
	return true;
}

int BackgroundStatusRefresh::getTimeout(uint32_t now) const
{
	if (!mActive)
		return -1;

	const uint32_t remaining = mNextDue - now;
	if (remaining >= 0x80000000u)
		return 0;

	return static_cast<int>(remaining);
}

void BackgroundStatusRefresh::reset()
{
	mActive = false;
	mRootIdentity = nullptr;
	mNextDue = 0;
}

bool GuiComponent::refreshStatus()
{
	return false;
}

bool GuiComponent::refreshBackgroundStatus()
{
	return refreshBackgroundStatusTree(this,
		[](GuiComponent* component) { return component->refreshStatus(); },
		[](GuiComponent* component) -> const std::vector<GuiComponent*>& { return component->getChildren(); },
		[](GuiComponent* child) { return child->refreshBackgroundStatus(); });
}
