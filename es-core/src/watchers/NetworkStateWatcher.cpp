#include "NetworkStateWatcher.h"
#include "components/IExternalActivity.h"
#include "utils/Platform.h"

NetworkStateWatcher::NetworkStateWatcher()
{
	mIsPlaneModeSupported = IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isReadPlaneModeSupported();
}

bool NetworkStateWatcher::check()
{
	bool networkConnected = !Utils::Platform::queryIPAddress().empty();
	bool planemodeEnabled = mIsPlaneModeSupported && IExternalActivity::Instance != nullptr && IExternalActivity::Instance->isPlaneMode();

	const auto previous = mState.get();
	bool changed = networkConnected != previous.connected || previous.planeMode != planemodeEnabled;
	mState.set(State{networkConnected, planemodeEnabled});
	
	return changed;
}
