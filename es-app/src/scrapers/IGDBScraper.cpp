#include "scrapers/IGDBScraper.h"
#include <exception>
#include <map>
#include <regex>
#include "FileData.h"
#include "Log.h"
#include "PlatformId.h"
#include "Settings.h"
#include "SystemData.h"
#include "utils/TimeUtil.h"
#include "utils/StringUtil.h"
#include "Genres.h"
#include "SystemConf.h"
#include "ApiSystem.h"
#include "LocaleES.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

using namespace PlatformIds;
using namespace rapidjson;

const std::map<PlatformId, std::string> igdb_platformids
{
	{ ARCADE, "52" }, // Has special filtering
	{ TEKNOPARROT, "52" },
	{ THREEDO, "50" },
	{ NINTENDO_3DS, "37" },
	{ ADAM, "68" },
	{ AMIGA, "16" },
	{ AMIGACD32, "114" },
	{ AMIGACDTV, "158" },
	{ AMIGA, "16" },
	{ AMSTRAD_CPC, "25" },
	{ APPLE2GS, "115" },
	{ APPLE_II, "75" },
	{ ARCHIMEDES, "116" },
	{ ASTROCADE, "91" },
	{ ATARI_2600, "59" },
	{ ATARI_5200, "66" },
	{ ATARI_7800, "60" },
	{ ATARI_800, "89769" },
	{ ATARI_JAGUAR, "62" },
	{ ATARI_JAGUAR_CD, "410" },
	{ ATARI_LYNX, "61" },
	{ ATARI_ST, "63" },
	{ ATARI_XE, "65" },
	{ ATOMISWAVE, "52" },
	{ ACORN_ATOM, "116" },
	{ ACORN_BBC_MICRO, "69" },
	{ ACORN_ELECTRON, "134"},
	{ VIC20, "71" },
	{ COMMODORE_64, "15" },
	{ PHILIPS_CDI, "117" },
	{ CHANNELF, "127" },
	{ COLECOVISION, "68" },
	{ PC, "13" },
	{ MOONLIGHT, "13,6" }, // "Windows"	
	{ SEGA_DREAMCAST, "23" },
	{ FMTOWNS, "118" },
	{ NINTENDO_GAME_AND_WATCH, "307" },
	{ NINTENDO_GAMECUBE, "21" },
	{ SEGA_GAME_GEAR, "35" },
	{ GAME_BOY_ADVANCE, "24" },
	{ GAME_BOY_COLOR, "22" },
	{ GAME_BOY, "33" },
	{ SEGA_GENESIS, "29" },
	{ GX4000, "25" },
	{ INTELLIVISION, "67" },
	{ SEGA_MASTER_SYSTEM, "64" },
	{ SEGA_PICO, "339" },
	{ THOMSON_TO_MO, "156" },
	{ MSX, "27" },
	{ NINTENDO_64, "4" },
	{ NINTENDO_64_DISK_DRIVE, "4" },
	{ NINTENDO_DS, "20" },
	{ NEOGEO_CD, "136" },
	{ NEOGEO, "79" },
	{ NINTENDO_ENTERTAINMENT_SYSTEM, "18" },
	{ NEOGEO_POCKET_COLOR, "120" },
	{ NEOGEO_POCKET, "119" },
	{ PCFX, "274" },
	{ PC_88, "125" },
	{ PC_98, "149" },
	{ TURBOGRAFX_16, "86" },
	{ TURBOGRAFX_CD, "150" },
	{ COMMODORE_PET, "90" },
	{ COMMODORE_PLUS4, "94" },
	{ PLAYSTATION_3, "9" },
	{ PLAYSTATION_2, "8" },
	{ PLAYSTATION_VITA, "46" },
	{ PLAYSTATION_PORTABLE, "38" },
	{ PLAYSTATION, "131" },
	{ SATELLAVIEW, "306" },
	{ SEGA_SATURN, "32" },
	{ SUPER_CASSETTE_VISION, "376" },
	{ SCUMMVM, "6" },
	{ SEGA_32X, "30" },
	{ SEGA_CD, "78" },
	{ SEGA_SG1000, "84" },
	{ SUPER_NINTENDO, "19" },
	{ SUPER_NINTENDO_MSU1, "19" },
	{ WATARA_SUPERVISION, "415" },
	{ SUPERGRAFX, "128" },
	{ NINTENDO_SWITCH, "130" },
	{ TI99, "129" },
	{ TRS80_COLOR_COMPUTER, "126" },
	{ VECTREX, "70" },
	{ VIDEOPAC_ODYSSEY2, "133" },
	{ NINTENDO_VIRTUAL_BOY, "87" },
	{ NINTENDO_WII_U, "41" },
	{ NINTENDO_WII, "5" },
	{ WONDERSWAN_COLOR, "123" },
	{ WONDERSWAN, "57" },
	{ SHARP_X1, "77" },
	{ SHARP_X6800, "121" },
	{ XBOX_360, "12" },
	{ XBOX, "11" },
	{ ZX81, "373" },
	{ ZX_SPECTRUM, "2" },
	{ FUJITSU_FM7, "152" },
	{ SEGA_CHIHIRO, "52"}
};

const std::set<Scraper::ScraperMediaSource>& IGDBScraper::getSupportedMedias()
{
	static std::set<ScraperMediaSource> mdds =
	{
		ScraperMediaSource::Screenshot,
		ScraperMediaSource::Box2d,
		ScraperMediaSource::FanArt
	};

	return mdds;
}

bool IGDBScraper::ensureToken()
{
	if (!mToken.empty() && Utils::Time::DateTime::now().elapsedSecondsSince(mTokenDate) < 1800) // make Token expire after 30mn
		return true;

	mToken = "";

	if (Settings::getInstance()->getString("IGDBClientID").empty() || Settings::getInstance()->getString("IGDBSecret").empty())
		return false;

	HttpReqOptions options;
	options.customHeaders.push_back("Content-Type: application/x-www-form-urlencoded");
	options.dataToPost = "client_id=" + Settings::getInstance()->getString("IGDBClientID") + "&client_secret=" + Settings::getInstance()->getString("IGDBSecret") + "&grant_type=client_credentials";

	HttpReq request("https://id.twitch.tv/oauth2/token", &options);
	if (request.wait())
	{
		Document doc;
		auto json = request.getContent();
		doc.Parse(json.c_str());

		if (!doc.HasParseError() && doc.HasMember("access_token"))
		{
			mToken = doc["access_token"].GetString();
			return true;
		}
	}

	return false;
}

void IGDBScraper::generateRequests(const ScraperSearchParams& params, std::queue<std::unique_ptr<ScraperRequest>>& requests, std::vector<ScraperSearchResult>& results)
{
	if (!ensureToken())
	{
		if (!params.isManualScrape)
			throw std::runtime_error(_("INVALID IGDB CREDENTIALS"));

		return;
	}

	mTokenDate = Utils::Time::DateTime::now();

	std::string cleanName = params.nameOverride;
	if (cleanName.empty())
		cleanName = Utils::String::removeParenthesis(Utils::FileSystem::getStem(params.game->getPath()));

	const std::string fields = "fields id, name, game_type.type, parent_game, platforms.name, genres.name, game_modes.name, multiplayer_modes.offlinemax, release_dates.date, release_dates.region, release_dates.platform, cover.image_id, screenshots.image_id, artworks.image_id, url, summary, aggregated_rating, involved_companies.company.name, involved_companies.developer, involved_companies.publisher ;";
	const std::string data = fields + " search \"" + cleanName + "\"; limit 10;";

	std::string fallbackData;
	if (params.nameOverride.empty())
	{
		// Numbered ROM sets prefix titles with a catalogue number, e.g. "1633 - ".
		static const std::regex cataloguePrefix(R"(^[0-9]+\s+-\s+)");
		const std::string fallbackName = Utils::String::trim(std::regex_replace(cleanName, cataloguePrefix, ""));
		if (!fallbackName.empty() && fallbackName != cleanName)
			fallbackData = fields + " search \"" + fallbackName + "\"; limit 10;";
	}

	std::string url = "https://api.igdb.com/v4/games";

	HttpReqOptions tokenAuth;
	tokenAuth.customHeaders.push_back("Client-ID: " + Settings::getInstance()->getString("IGDBClientID"));
	tokenAuth.customHeaders.push_back("Authorization: Bearer " + mToken);
	tokenAuth.customHeaders.push_back("Content-Type: text/plain");
	tokenAuth.customHeaders.push_back("Accept: application/json");

	auto queueRequest = [&](const std::string& platformFilter)
	{
		HttpReqOptions options = tokenAuth;
		options.dataToPost = data + platformFilter;
		if (!fallbackData.empty())
			requests.push(std::unique_ptr<ScraperRequest>(new IGDBRequest(requests, results, url, &options, params.system->hasPlatformId(PlatformIds::ARCADE), params.isManualScrape, fallbackData + platformFilter)));
		else
			requests.push(std::unique_ptr<ScraperRequest>(new IGDBRequest(requests, results, url, &options, params.system->hasPlatformId(PlatformIds::ARCADE), params.isManualScrape)));
	};

	auto& platforms = params.system->getPlatformIds();
	if (!platforms.empty())
	{
		for (auto platform : platforms)
		{
			auto it = igdb_platformids.find(platform);
			if (it != igdb_platformids.cend())
			{
				for (auto plaformId : Utils::String::split(it->second, ',', true))
					queueRequest(" where release_dates.platform = " + plaformId + ";");
			}
		}
	}

	if (requests.size() == 0)
		queueRequest("");
}

bool IGDBScraper::isSupportedPlatform(SystemData* system)
{
	std::string platformQueryParam;
	auto& platforms = system->getPlatformIds();

	for (auto platform : platforms)
		if (igdb_platformids.find(platform) != igdb_platformids.cend())
			return true;

	return false;
}

static bool isIgdbPort(const Value& game)
{
	if (!game.HasMember("game_type") || !game["game_type"].IsObject())
		return false;

	const auto& gameType = game["game_type"];
	return gameType.HasMember("type") && gameType["type"].IsString() && std::string(gameType["type"].GetString()) == "Port";
}

static bool hasIgdbMedia(const ScraperSearchResult& result, MetaDataId media)
{
	auto it = result.urls.find(media);
	return it != result.urls.end() && !it->second.url.empty();
}

static void processIgdbMedia(const Value& game, ScraperSearchResult& result)
{
	if (!hasIgdbMedia(result, MetaDataId::Thumbnail) && game.HasMember("cover") && game["cover"].IsObject())
	{
		const auto& cover = game["cover"];
		if (cover.HasMember("image_id") && cover["image_id"].IsString() && cover["image_id"].GetStringLength() > 0)
			result.urls[MetaDataId::Thumbnail] = "https://images.igdb.com/igdb/image/upload/t_original/" + std::string(cover["image_id"].GetString()) + ".jpg";
	}

	if (!hasIgdbMedia(result, MetaDataId::Image) && game.HasMember("artworks") && game["artworks"].IsArray())
	{
		for (const auto& artwork : game["artworks"].GetArray())
		{
			if (!artwork.IsObject() || !artwork.HasMember("image_id") || !artwork["image_id"].IsString() || artwork["image_id"].GetStringLength() == 0)
				continue;

			result.urls[MetaDataId::Image] = "https://images.igdb.com/igdb/image/upload/t_1080p/" + std::string(artwork["image_id"].GetString()) + ".jpg";
			break;
		}
	}

	if (Settings::getInstance()->getBool("ScrapeFanart") && !hasIgdbMedia(result, MetaDataId::FanArt) && game.HasMember("screenshots") && game["screenshots"].IsArray())
	{
		for (const auto& screenshot : game["screenshots"].GetArray())
		{
			if (!screenshot.IsObject() || !screenshot.HasMember("image_id") || !screenshot["image_id"].IsString() || screenshot["image_id"].GetStringLength() == 0)
				continue;

			result.urls[MetaDataId::FanArt] = "https://images.igdb.com/igdb/image/upload/t_screenshot_huge/" + std::string(screenshot["image_id"].GetString()) + ".jpg";
			break;
		}
	}
}

static void processIgdbGame(const Value& game, std::vector<ScraperSearchResult>& results, bool arcadeGame)
{
	ScraperSearchResult result("IGDB");
	if (game.HasMember("name") && game["name"].IsString())
		result.mdl.set(MetaDataId::Name, game["name"].GetString());

	if (game.HasMember("summary") && game["summary"].IsString())
		result.mdl.set(MetaDataId::Desc, game["summary"].GetString());

	result.mdl.set(MetaDataId::Rating, "-1");

	if (game.HasMember("involved_companies") && game["involved_companies"].IsArray())
	{
		const Value& involved_companies = game["involved_companies"];
		for (int i = 0; i < (int)involved_companies.Size(); ++i)
		{
			auto& involved_company = involved_companies[i];
			if (involved_company.HasMember("company") && involved_company["company"].IsObject() && involved_company["company"].HasMember("name") && involved_company["company"]["name"].IsString())
			{
				std::string companyName = involved_company["company"]["name"].GetString();
				if (!companyName.empty())
				{
					if (involved_company.HasMember("developer") && involved_company["developer"].IsBool() && involved_company["developer"].GetBool())
						result.mdl.set(MetaDataId::Developer, companyName);

					if (involved_company.HasMember("publisher") && involved_company["publisher"].IsBool() && involved_company["publisher"].GetBool())
						result.mdl.set(MetaDataId::Publisher, companyName);
				}
			}
		}
	}

	processIgdbMedia(game, result);

	/*
	if (Settings::getInstance()->getBool("ScrapeVideos") && game.HasMember("videos") && game["videos"].IsArray())
	{
		const Value& videos = game["videos"];
		for (int i = 0; i < (int)videos.Size(); ++i)
		{
			auto& video = videos[i];
			if (!video.HasMember("video_id") || !video["video_id"].IsString())
				continue;

			result.urls[MetaDataId::Video] = "https://www.youtube.com/watch?v=" + std::string(video["video_id"].GetString());
			break;
		}
	}
	*/
	results.push_back(result);
}

// Process should return false only when we reached a maximum scrap by minute, to retry
bool IGDBRequest::process(const std::string& response, std::vector<ScraperSearchResult>& results)
{
	Document doc;
	doc.Parse(response.c_str());

	if (doc.HasParseError())
	{
		std::string err = std::string("IGDBRequest - Error parsing JSON. \n\t") + GetParseError_En(doc.GetParseError());
		setError(err);
		LOG(LogError) << err;
		return false;
	}

	if (!doc.IsArray())
	{
		std::string warn = "IGDBRequest - Response is not valid.\n";
		LOG(LogWarning) << warn;
		return false;
	}

	if (doc.Empty())
	{
		if (!mFallbackQuery.empty())
		{
			LOG(LogDebug) << "IGDB: no matches; retrying without the ROM catalogue number";
			HttpReqOptions options = mRequestOptions;
			options.dataToPost = mFallbackQuery;
			mRequestQueue.push(std::unique_ptr<ScraperRequest>(new IGDBRequest(mRequestQueue, results, mUrl, &options, mIsArcade, mIsManualScrape)));
			mFallbackQuery.clear();
		}
		return true;
	}

	std::vector<ScraperSearchResult> pendingResults;
	IGDBParentMediaRequest::ParentResults parentResults;
	std::set<std::string> parentFields;
	for (int i = 0; i < (int)doc.Size(); ++i)
	{
		auto& v = doc[i];
		if (!v.IsObject())
			continue;

		try
		{
			processIgdbGame(v, pendingResults, mIsArcade);
			if (!isIgdbPort(v) ||
				!v.HasMember("parent_game") || !v["parent_game"].IsUint() || v["parent_game"].GetUint() == 0 ||
				(v.HasMember("id") && v["id"].IsUint() && v["id"].GetUint() == v["parent_game"].GetUint()))
				continue;

			const auto& result = pendingResults.back();
			const bool missingCover = !hasIgdbMedia(result, MetaDataId::Thumbnail);
			const bool missingArtwork = !hasIgdbMedia(result, MetaDataId::Image);
			const bool missingScreenshot = Settings::getInstance()->getBool("ScrapeFanart") && !hasIgdbMedia(result, MetaDataId::FanArt);
			if (!missingCover && !missingArtwork && !missingScreenshot)
				continue;

			parentResults[v["parent_game"].GetUint()].push_back(pendingResults.size() - 1);
			if (missingCover)
				parentFields.insert("cover.image_id");
			if (missingArtwork)
				parentFields.insert("artworks.image_id");
			if (missingScreenshot)
				parentFields.insert("screenshots.image_id");
		}
		catch (std::runtime_error& e)
		{
			LOG(LogError) << "Error while processing game: " << e.what();
		}
	}

	if (parentResults.empty())
		results.insert(results.end(), pendingResults.begin(), pendingResults.end());
	else
	{
		// Fetch all missing media and distinct parents in one request, without the port's platform filter.
		HttpReqOptions options = mRequestOptions;
		options.dataToPost = "fields id";
		for (const auto& field : parentFields)
			options.dataToPost += ", " + field;
		options.dataToPost += "; where id = (";
		std::string separator;
		for (const auto& parent : parentResults)
		{
			options.dataToPost += separator + std::to_string(parent.first);
			separator = ",";
		}
		options.dataToPost += "); limit " + std::to_string(parentResults.size()) + ";";
		mRequestQueue.push(std::unique_ptr<ScraperRequest>(new IGDBParentMediaRequest(results, mUrl, &options,
			std::move(pendingResults), std::move(parentResults), mIsManualScrape)));
	}

	return true;
}

IGDBParentMediaRequest::IGDBParentMediaRequest(std::vector<ScraperSearchResult>& results, const std::string& url, HttpReqOptions* options,
	std::vector<ScraperSearchResult> pendingResults, ParentResults parentResults, bool isManualScrape)
	: ScraperHttpRequest(results, url, options), mPendingResults(std::move(pendingResults)),
	mParentResults(std::move(parentResults)), mIsManualScrape(isManualScrape)
{
}

void IGDBParentMediaRequest::update()
{
	if (mStatus != ASYNC_IN_PROGRESS)
		return;

	ScraperHttpRequest::update();
	if (mStatus == ASYNC_IN_PROGRESS)
		return;

	// Parent media is optional: preserve the port's results even if the lookup fails.
	if (mStatus == ASYNC_ERROR)
		LOG(LogWarning) << "IGDB parent media lookup failed: " << getStatusString();

	mResults.insert(mResults.end(), mPendingResults.begin(), mPendingResults.end());
	mPendingResults.clear();
	setStatus(ASYNC_DONE);
}

bool IGDBParentMediaRequest::process(const std::string& response, std::vector<ScraperSearchResult>& results)
{
	Document doc;
	doc.Parse(response.c_str());
	if (doc.HasParseError() || !doc.IsArray())
	{
		LOG(LogWarning) << "IGDB parent media response is not valid";
		return false;
	}

	for (const auto& parent : doc.GetArray())
	{
		if (!parent.IsObject() || !parent.HasMember("id") || !parent["id"].IsUint())
			continue;

		auto it = mParentResults.find(parent["id"].GetUint());
		if (it == mParentResults.end())
			continue;

		for (auto index : it->second)
			processIgdbMedia(parent, mPendingResults[index]);
	}
	return true;
}
