#include "components/BatteryIconComponent.h"
#include "Settings.h"
#include "watchers/BatteryLevelWatcher.h"

BatteryIconComponent::BatteryIconComponent(Window* window) : ImageComponent(window)
{	
	mIncharge = ResourceManager::getInstance()->getResourcePath(":/battery/incharge.svg");
	mFull = ResourceManager::getInstance()->getResourcePath(":/battery/full.svg");
	mAt75 = ResourceManager::getInstance()->getResourcePath(":/battery/75.svg");
	mAt50 = ResourceManager::getInstance()->getResourcePath(":/battery/50.svg");
	mAt25 = ResourceManager::getInstance()->getResourcePath(":/battery/25.svg");
	mEmpty = ResourceManager::getInstance()->getResourcePath(":/battery/empty.svg");

	WatchersManager::getInstance()->RegisterNotify(this);

	BatteryLevelWatcher* watcher = WatchersManager::GetComponent<BatteryLevelWatcher>();
	if (watcher != nullptr)
		mBatteryInfo.setInitial(watcher->getBatteryInfo());
}

BatteryIconComponent::~BatteryIconComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void BatteryIconComponent::OnWatcherChanged(IWatcher* component)
{
	BatteryLevelWatcher* watcher = dynamic_cast<BatteryLevelWatcher*>(component);
	if (watcher != nullptr)
	{
		mBatteryInfo.set(watcher->getBatteryInfo());
	}
}

void BatteryIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);
	updateBatteryStatus(false);
}

bool BatteryIconComponent::refreshStatus()
{
	return updateBatteryStatus(true);
}

bool BatteryIconComponent::updateBatteryStatus(bool resolveImage)
{
	return StatusComponents::refreshPresentation(*this, [this, resolveImage]()
	{
		const auto info = mBatteryInfo.get();
		setVisible(!Settings::getInstance()->getString("ShowBattery").empty() && info.hasBattery);
		if (isVisible())
		{
			std::string txName = mIncharge;
			if (info.isCharging && !mIncharge.empty())
				txName = mIncharge;
			else if (info.level > 75 && !mFull.empty())
				txName = mFull;
			else if (info.level > 50 && !mAt75.empty())
				txName = mAt75;
			else if (info.level > 25 && !mAt50.empty())
				txName = mAt50;
			else if (info.level > 5 && !mAt25.empty())
				txName = mAt25;
			else
				txName = mEmpty;
			setImage(txName);
		}
		return resolveImage && refreshBackgroundImage();
	});
}

void BatteryIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	ImageComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("incharge") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("incharge")))
		mIncharge = elem->get<std::string>("incharge");

	if (elem->has("full") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("full")))
		mFull = elem->get<std::string>("full");

	if (elem->has("at75") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at75")))
		mAt75 = elem->get<std::string>("at75");

	if (elem->has("at50") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at50")))
		mAt50 = elem->get<std::string>("at50");

	if (elem->has("at25") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("at25")))
		mAt25 = elem->get<std::string>("at25");

	if (elem->has("empty") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("empty")))
		mEmpty = elem->get<std::string>("empty");
}

