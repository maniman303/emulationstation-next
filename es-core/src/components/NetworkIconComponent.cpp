#include "components/NetworkIconComponent.h"
#include "Settings.h"
#include "utils/Platform.h"
#include "IExternalActivity.h"
#include "watchers/NetworkStateWatcher.h"

NetworkIconComponent::NetworkIconComponent(Window* window) : ImageComponent(window)
{
	setVisible(false);

	WatchersManager::getInstance()->RegisterNotify(this);
	NetworkStateWatcher* watcher = WatchersManager::GetComponent<NetworkStateWatcher>();
	if (watcher != nullptr)
	{
		mNetworkInfo.setInitial(watcher->getStatus());
	}
}

NetworkIconComponent::~NetworkIconComponent()
{
	WatchersManager::getInstance()->UnregisterNotify(this);
}

void NetworkIconComponent::OnWatcherChanged(IWatcher* component)
{
	NetworkStateWatcher* watcher = dynamic_cast<NetworkStateWatcher*>(component);
	if (watcher != nullptr)
	{
		mNetworkInfo.set(watcher->getStatus());
	}
}

void NetworkIconComponent::update(int deltaTime)
{
	ImageComponent::update(deltaTime);
	updateNetworkStatus(false);
}

bool NetworkIconComponent::refreshStatus()
{
	return updateNetworkStatus(true);
}

bool NetworkIconComponent::updateNetworkStatus(bool resolveImage)
{
	return StatusComponents::refreshPresentation(*this, [this, resolveImage]()
	{
		const auto info = mNetworkInfo.get();
		setVisible(Settings::ShowNetworkIndicator() && (info.connected || info.planeMode));
		if (isVisible())
			setImage(info.planeMode && !mPlanemodeIcon.empty() ? mPlanemodeIcon : mNetworkIcon);
		return resolveImage && refreshBackgroundImage();
	});
}

void NetworkIconComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	ImageComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, getThemeTypeName());
	if (!elem)
		return;

	if (elem->has("networkIcon") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("networkIcon")))
		mNetworkIcon = elem->get<std::string>("networkIcon");
	else if (elem->has("path") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("path")))
		mNetworkIcon = elem->get<std::string>("path");

	if (elem->has("planemodeIcon") && ResourceManager::getInstance()->fileExists(elem->get<std::string>("planemodeIcon")))
		mPlanemodeIcon = elem->get<std::string>("planemodeIcon");
}
