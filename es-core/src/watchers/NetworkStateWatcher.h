#pragma once

#include "WatchersManager.h"
#include "StatusComponentState.h"

class NetworkStateWatcher : public IWatcher
{
public:
	struct State
	{
		bool connected = false;
		bool planeMode = false;
	};

	NetworkStateWatcher();

	State getStatus() const { return mState.get(); }
	bool isConnected() const { return getStatus().connected; }
	bool isPlaneMode() const { return getStatus().planeMode; }

protected:
	bool enabled() override { return true; };

	int  initialUpdateTime() override { return 0; }		// Immediate
	int  updateTime() override { return 5 * 1000; }		// 5 seconds

	bool check() override;

private:
	StatusComponents::LatestReading<State> mState;
	bool mIsPlaneModeSupported;
};
