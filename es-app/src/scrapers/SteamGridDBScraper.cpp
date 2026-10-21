#include "scrapers/SteamGridDBScraper.h"
#include "scrapers/SteamGridDBClient.h"

#include "LocaleES.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>

namespace SteamGridDB
{

static const MetaDataId mediaIds[] = { MetaDataId::Image, MetaDataId::Thumbnail, MetaDataId::Marquee };
static const char* const mediaSettings[] = { "SteamGridDBImageSource", "SteamGridDBBoxSource", "SteamGridDBLogoSource" };
using Clock = std::chrono::steady_clock;

static std::string apiKey()
{
	return Utils::String::trim(Settings::getInstance()->getString("SteamGridDBApiKey"));
}

static std::string keyError()
{
	return _("STEAMGRIDDB API KEY IS MISSING. ENTER IT IN SCRAPER > ACCOUNTS.");
}

static std::string errorMessage(SteamGridDB::Error error)
{
	switch (error)
	{
	case SteamGridDB::Error::MissingKey: return keyError();
	case SteamGridDB::Error::InvalidKey: return _("STEAMGRIDDB API KEY IS INVALID. CHECK SCRAPER > ACCOUNTS.");
	case SteamGridDB::Error::Network: return _("COULD NOT CONNECT TO STEAMGRIDDB. CHECK YOUR INTERNET CONNECTION.");
	case SteamGridDB::Error::RateLimited: return _("STEAMGRIDDB REQUEST LIMIT REACHED. PLEASE TRY AGAIN LATER.");
	case SteamGridDB::Error::ServiceUnavailable: return _("STEAMGRIDDB IS UNAVAILABLE. PLEASE TRY AGAIN LATER.");
	default: return _("STEAMGRIDDB RETURNED AN INVALID RESPONSE. PLEASE TRY AGAIN LATER.");
	}
}

// Share successful API pages between ROM variants and repeated manual searches.
// Entries are bounded, expire after ten minutes, and never survive a key change.
struct ResponseCache
{
	struct Entry { Clock::time_point time; std::string body; };
	std::mutex mutex;
	std::string key;
	std::map<std::string, Entry> entries;

	void selectKey(const std::string& value)
	{
		if (key != value) { key = value; entries.clear(); }
	}

	bool get(const std::string& credential, const std::string& path, std::string& body)
	{
		std::lock_guard<std::mutex> lock(mutex);
		selectKey(credential);
		auto item = entries.find(path);
		if (item == entries.end()) return false;
		if (Clock::now() - item->second.time > std::chrono::minutes(10))
		{
			entries.erase(item);
			return false;
		}
		body = item->second.body;
		return true;
	}

	void put(const std::string& credential, const std::string& path, const std::string& body)
	{
		std::lock_guard<std::mutex> lock(mutex);
		selectKey(credential);
		if (entries.size() >= 128)
		{
			auto oldest = std::min_element(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.second.time < b.second.time; });
			entries.erase(oldest);
		}
		entries[path] = { Clock::now(), body };
	}
};

static ResponseCache cache;

class SteamGridDBRequest : public ScraperRequest
{
public:
	SteamGridDBRequest(std::vector<ScraperSearchResult>& results, const std::string& credential,
		const std::string& name, bool explicitName, bool manual, const std::array<bool, 3>& enabled, bool verticalImage, bool preferScore)
		: ScraperRequest(results), mKey(credential), mClient(name, explicitName, manual, enabled, verticalImage, preferScore)
	{
		const auto validation = SteamGridDB::checkKey(mKey);
		if (validation != SteamGridDB::Error::None)
		{
			setError(HttpReq::REQ_401_FORBIDDEN, errorMessage(validation));
			return;
		}
		mOptions.useCookieManager = false;
		mOptions.connectTimeout = 10000;
	}

	void update() override
	{
		if (mStatus != ASYNC_IN_PROGRESS) return;
		if (!mClient.done())
		{
			const auto path = mClient.path();
			const auto bodyToPost = mClient.body();
			const auto cacheId = path + "\n" + bodyToPost;
			if (!mRequest)
			{
				std::string body;
				if (cache.get(mKey, cacheId, body))
				{
					mClient.receive(200, body);
					return; // At most one page per update, including cached pages.
				}
				mStarted = Clock::now();
				mOptions.dataToPost = bodyToPost;
				mOptions.customHeaders = { "Accept: application/json", "Content-Type: application/json" };
				if (bodyToPost.empty()) mOptions.customHeaders.push_back("Authorization: Bearer " + mKey);
				else mOptions.customHeaders.push_back("Referer: https://www.steamgriddb.com/");
				mRequest.reset(new HttpReq("https://www.steamgriddb.com" + path, &mOptions));
			}
			const auto status = mRequest->status();
			if (status == HttpReq::REQ_IN_PROGRESS)
			{
				if (Clock::now() - mStarted < std::chrono::seconds(20)) return;
				mClient.receive(HttpReq::REQ_IO_ERROR, ""); // Includes a stalled response body.
			}
			else
			{
				const auto body = status == HttpReq::REQ_SUCCESS ? mRequest->getContent() : "";
				mClient.receive(status, body);
				if (status == HttpReq::REQ_SUCCESS && mClient.error() == SteamGridDB::Error::None) cache.put(mKey, cacheId, body);
			}
			mRequest.reset();
		}
		if (!mClient.done()) return;
		if (mClient.error() != SteamGridDB::Error::None)
		{
			setError(mClient.errorCode(), errorMessage(mClient.error()));
			return;
		}
		for (const auto& game : mClient.results())
		{
			ScraperSearchResult result("SteamGridDB");
			result.mdl.set(MetaDataId::Name, game.name);
			for (size_t i = 0; i < game.artwork.size(); ++i)
				if (!game.artwork[i].url.empty()) result.urls[mediaIds[i]] = ScraperSearchItem(game.artwork[i].url, game.artwork[i].extension);
			mResults.push_back(result);
		}
		setStatus(ASYNC_DONE);
	}

private:
	std::string mKey;
	HttpReqOptions mOptions;
	SteamGridDB::Client mClient;
	std::unique_ptr<HttpReq> mRequest;
	Clock::time_point mStarted;
};

} // namespace SteamGridDB

const std::set<Scraper::ScraperMediaSource>& SteamGridDBScraper::getSupportedMedias()
{
	static const std::set<ScraperMediaSource> media = { Box2d, Screenshot, Marquee };
	return media;
}

int SteamGridDBScraper::getThreadCount(std::string& error)
{
	const auto validation = SteamGridDB::checkKey(SteamGridDB::apiKey());
	if (validation != SteamGridDB::Error::None) { error = SteamGridDB::errorMessage(validation); return -1; }
	return 1;
}

bool SteamGridDBScraper::hasAnyMedia(FileData* file)
{
	for (size_t i = 0; i < 3; ++i)
		if (!Settings::getInstance()->getString(SteamGridDB::mediaSettings[i]).empty() && Utils::FileSystem::exists(file->getMetadata(SteamGridDB::mediaIds[i]))) return true;
	return false;
}

bool SteamGridDBScraper::hasMissingMedia(FileData* file)
{
	for (size_t i = 0; i < 3; ++i)
		if (!Settings::getInstance()->getString(SteamGridDB::mediaSettings[i]).empty() && !Utils::FileSystem::exists(file->getMetadata(SteamGridDB::mediaIds[i]))) return true;
	return false;
}

void SteamGridDBScraper::generateRequests(const ScraperSearchParams& params,
	std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	const std::string credential = SteamGridDB::apiKey();
	const auto name = params.nameOverride.empty() ? Utils::FileSystem::getStem(params.game->getPath()) : params.nameOverride;
	const bool overwrite = params.overWriteMedias && Settings::getInstance()->getBool("ScrapeOverWrite");
	std::array<bool, 3> enabled;
	for (size_t i = 0; i < enabled.size(); ++i)
		enabled[i] = !Settings::getInstance()->getString(SteamGridDB::mediaSettings[i]).empty() &&
			(params.isManualScrape || overwrite || !Utils::FileSystem::exists(params.game->getMetadata(SteamGridDB::mediaIds[i])));
	const bool verticalImage = Settings::getInstance()->getString("SteamGridDBImageSource") == "sgdb-vertical";
	const bool preferScore = Settings::getInstance()->getString("SteamGridDBAssetRanking") == "score";
	requests.push(std::unique_ptr<ScraperRequest>(new SteamGridDB::SteamGridDBRequest(results, credential, name, !params.nameOverride.empty(), params.isManualScrape, enabled, verticalImage, preferScore)));
}
