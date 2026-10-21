#include "components/BatteryTextComponent.h"
#include "Settings.h"
#include "watchers/BatteryLevelWatcher.h"

BatteryTextComponent::BatteryTextComponent(Window* window) : TextComponent(window)
{
	WatchersManager::getInstance()->RegisterNotify(this);
	BatteryLevelWatcher* watcher = WatchersManager::GetComponent<BatteryLevelWatcher>();
	if (watcher != nullptr)
		mBatteryInfo.setInitial(watcher->getBatteryInfo());
}

BatteryTextComponent::~BatteryTextComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void BatteryTextComponent::OnWatcherChanged(IWatcher* component)
{
	BatteryLevelWatcher* watcher = dynamic_cast<BatteryLevelWatcher*>(component);
	if (watcher != nullptr)
	{
		mBatteryInfo.set(watcher->getBatteryInfo());
	}
}

void BatteryTextComponent::update(int deltaTime)
{
	TextComponent::update(deltaTime);
	updateBatteryStatus();
}

bool BatteryTextComponent::refreshStatus()
{
	return updateBatteryStatus();
}

bool BatteryTextComponent::updateBatteryStatus()
{
	return StatusComponents::refreshPresentation(*this, [this]()
	{
		const auto info = mBatteryInfo.get();
		setVisible(Settings::getInstance()->getString("ShowBattery") == "text" && info.hasBattery && info.level >= 0);
		if (isVisible())
		{
			auto batteryText = std::to_string(info.level) + "%";
			if (getValue() == batteryText)
				return false;
			float sx = mSize.x();
			mAutoCalcExtent.x() = 1;
			setText(batteryText);
			if (mSize.x() != sx)
			{
				auto sz = mSize;
				mSize.x() = sx;
				setSize(sz);
			}
		}
		return false;
	});
}
