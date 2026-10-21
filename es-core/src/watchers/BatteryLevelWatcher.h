#pragma once

#include "WatchersManager.h"
#include "utils/Platform.h"
#include "StatusComponentState.h"

class BatteryLevelWatcher : public IWatcher
{
public:
	Utils::Platform::BatteryInformation getBatteryInfo() const { return mBatteryInfo.get(); }

protected:
	bool enabled() override { return true; };

	int  initialUpdateTime() override { return 0; }		// Immediate
	int  updateTime() override { return 5 * 1000; }		// 5 seconds

	bool check() override;

private:
	StatusComponents::LatestReading<Utils::Platform::BatteryInformation> mBatteryInfo;
};
