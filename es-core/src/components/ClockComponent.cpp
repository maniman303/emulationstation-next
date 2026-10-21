#include "components/ClockComponent.h"
#include "Settings.h"
#include "StatusComponentState.h"
#include <time.h>

ClockComponent::ClockComponent(Window* window) : TextComponent(window)
{
	mClockElapsed = 0;
}

void ClockComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);

	setVisible(Settings::getInstance()->getBool("DrawClock"));
	if (!isVisible())
		return;

	mClockElapsed -= deltaTime;
	if (mClockElapsed <= 0)
	{
		updateClockText();
		mClockElapsed = 1000;
	}
}

bool ClockComponent::refreshStatus()
{
	return StatusComponents::refreshPresentation(*this, [this]()
	{
		setVisible(Settings::DrawClock());
		if (isVisible())
			updateClockText();
		return false;
	});
}

bool ClockComponent::updateClockText()
{
	time_t clockNow = time(0);
	struct tm clockTstruct = *localtime(&clockNow);
	if (clockTstruct.tm_year <= 100)
		return false;

	const std::string previous = getValue();
	std::string clockBuf;
	if (Settings::getInstance()->getBool("ClockMode12"))
		clockBuf = Utils::Time::timeToString(clockNow, "%I:%M %p");
	else
		clockBuf = Utils::Time::timeToString(clockNow, "%H:%M");
	if (previous == clockBuf)
		return false;

	float sx = mSize.x();
	mAutoCalcExtent.x() = 1;
	setText(clockBuf);
	if (mSize.x() != sx)
	{
		auto sz = mSize;
		mSize.x() = sx;
		setSize(sz);
	}

	return previous != clockBuf;
}
