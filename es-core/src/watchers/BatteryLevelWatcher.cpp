#include "BatteryLevelWatcher.h"

bool BatteryLevelWatcher::check()
{
	auto batteryInfo = Utils::Platform::queryBatteryInformation();
	const auto previous = mBatteryInfo.get();
	bool changed = batteryInfo.hasBattery != previous.hasBattery || batteryInfo.isCharging != previous.isCharging || batteryInfo.level != previous.level;
	mBatteryInfo.set(batteryInfo);
	return changed;
}
