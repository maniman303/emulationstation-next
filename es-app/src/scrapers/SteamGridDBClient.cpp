#include "scrapers/SteamGridDBClient.h"
#include "scrapers/GameSearchNames.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <rapidjson/document.h>

namespace SteamGridDB
{

static std::string encode(const std::string& text)
{
	const char* hex = "0123456789ABCDEF";
	std::string result;
	for (unsigned char c : text)
	{
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') result += static_cast<char>(c);
		else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
	}
	return result;
}

static bool number(const rapidjson::Value& object, const char* name, uint64_t& value)
{
	if (!object.IsObject() || !object.HasMember(name) || !object[name].IsUint64()) return false;
	value = object[name].GetUint64();
	return true;
}

static bool better(const Artwork& a, const Artwork& b, bool preferScore)
{
	if (b.url.empty()) return true;
	if (preferScore && a.score != b.score) return a.score > b.score;
	if (a.hearts != b.hearts) return a.hearts > b.hearts;
	if (a.pixels != b.pixels) return a.pixels > b.pixels;
	return a.id < b.id;
}

std::vector<std::string> searchNames(const std::string& name, bool explicitName)
{
	return GameSearchNames::candidates(name, explicitName);
}

double titleRelevance(const std::string& query, const std::string& title)
{
	return GameSearchNames::relevance(query, title);
}

Error checkKey(const std::string& trimmedKey)
{
	if (trimmedKey.empty()) return Error::MissingKey;
	for (unsigned char c : trimmedKey)
		if (c <= 0x20 || c >= 0x7f) return Error::InvalidKey;
	return Error::None; // Key length/format is validated by the API.
}

Client::Client(const std::string& name, bool explicitName, bool manual, const std::array<bool, 3>& enabled, bool verticalImage, bool preferScore, const std::vector<std::string>& knownNames)
	: mName(name), mExplicitName(explicitName), mQueries(GameSearchNames::candidates(name, explicitName, knownNames)), mManual(manual), mVerticalImage(verticalImage), mPreferScore(preferScore), mEnabled(enabled)
{
	if (mQueries.empty()) mDone = true;
}

bool Client::needsAliases() const
{
	return !mSearching && mError == Error::None && (mGames.empty() || mGames.front().relevance < .95);
}

bool Client::addAliases(const std::vector<std::string>& names)
{
	auto combined = GameSearchNames::candidates(mName, mExplicitName, names);
	// Continue only with new names; do not repeat completed filename searches.
	for (const auto& name : mQueries)
		combined.erase(std::remove(combined.begin(), combined.end(), name), combined.end());
	const auto remaining = GameSearchNames::MaxCandidates - mQueries.size();
	if (combined.size() > remaining) combined.resize(remaining);
	if (combined.empty()) return false;
	mQuery = mQueries.size();
	mQueries.insert(mQueries.end(), combined.begin(), combined.end());
	mGames.clear();
	mGame = mArtwork = mPage = mSeen = 0;
	mSearching = true;
	mDone = false;
	return true;
}

std::string Client::path() const
{
	if (mDone) return {};
	if (mSearching) return "/api/v2/search/autocomplete/" + encode(mQueries[mQuery]);
	return "/api/public/search/assets";
}

std::string Client::body() const
{
	if (mDone || mSearching) return {};
	// v2's score/upvotes fields do not represent the website's hearts. The
	// site's public read-only artwork search returns hearts for a whole page,
	// avoiding an extra detail request for every individual image.
	std::string body = "{\"asset_type\":\"";
	body += mArtwork == 2 ? "logo" : "grid";
	body += "\",\"game_id\":[" + std::to_string(mGames[mGame].id) + "],\"styles\":[\"all\"],\"languages\":[\"all\"],\"dimensions\":[";
	if (mArtwork == 0 && mVerticalImage) body += "\"600x900\"";
	else if (mArtwork == 0) body += "\"460x215\",\"920x430\"";
	else if (mArtwork == 1) body += "\"512x512\",\"1024x1024\"";
	else body += "\"all\"";
	body += "],\"formats\":[\"all\"],\"mime\":[\"image/png\"";
	if (mArtwork != 2) body += ",\"image/jpeg\"";
	body += "],\"order\":\"age_asc\",\"static\":true,\"animated\":false,\"nsfw\":false,\"humor\":false,\"epilepsy\":false,\"untagged\":true,\"limit\":48,\"page\":" + std::to_string(mPage) + "}";
	return body;
}

void Client::fail(Error error, int status)
{
	mError = error;
	mErrorCode = status;
	mDone = true;
	mGames.clear(); // A partial lookup is never reported as a successful scrape.
}

void Client::finishSearch()
{
	mSearching = false;
	std::stable_sort(mGames.begin(), mGames.end(), [](const Game& a, const Game& b) { return a.relevance > b.relevance; });
	if (mGames.size() > (mManual ? 5u : 1u)) mGames.resize(mManual ? 5 : 1);
	if (mGames.empty()) { mDone = true; return; }
	while (mArtwork < 3 && !mEnabled[mArtwork]) ++mArtwork;
	if (mArtwork == 3) mDone = true;
}

void Client::nextArtwork()
{
	mPage = 0;
	mSeen = 0;
	do
	{
		if (++mArtwork == 3) { mArtwork = 0; ++mGame; }
		if (mGame == mGames.size()) { mDone = true; return; }
	} while (!mEnabled[mArtwork]);
}

void Client::receive(int status, const std::string& body)
{
	if (mDone) return;
	if (status == 401 || status == 403) { fail(Error::InvalidKey, status); return; }
	if (status == 429) { fail(Error::RateLimited, status); return; }
	if (status == 0 || status == 3 || status == 408) { fail(Error::Network, 3); return; }
	if (status == 404)
	{
		if (mSearching)
		{
			if (++mQuery == mQueries.size()) finishSearch();
		}
		else fail(Error::InvalidResponse, status);
		return;
	}
	if (status >= 500) { fail(Error::ServiceUnavailable, status); return; }
	if (status != 200) { fail(Error::InvalidResponse, status); return; }

	rapidjson::Document doc;
	doc.Parse(body.c_str());
	if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("success") ||
		!doc["success"].IsBool() || !doc["success"].GetBool() || !doc.HasMember("data") ||
		(mSearching ? !doc["data"].IsArray() : (!doc["data"].IsObject() || !doc["data"].HasMember("assets") || !doc["data"]["assets"].IsArray())))
	{
		fail(Error::InvalidResponse, 500);
		return;
	}
	const auto& rows = mSearching ? doc["data"] : doc["data"]["assets"];
	if (mSearching)
	{
		bool strongMatch = false;
		for (const auto& row : rows.GetArray())
		{
			uint64_t id;
			if (!number(row, "id", id) || !id || !row.HasMember("name") || !row["name"].IsString())
			{
				fail(Error::InvalidResponse, 500);
				return;
			}
			Game game;
			game.id = id;
			game.name = row["name"].GetString();
			for (const auto& query : mQueries) game.relevance = std::max(game.relevance, titleRelevance(query, game.name));
			if (game.relevance < (mManual ? 0.5 : 0.75)) continue;
			strongMatch |= game.relevance >= 0.95;
			auto previous = std::find_if(mGames.begin(), mGames.end(), [id](const Game& value) { return value.id == id; });
			if (previous == mGames.end()) mGames.push_back(game);
		}
		if (strongMatch || ++mQuery == mQueries.size()) finishSearch();
		return;
	}

	for (const auto& row : rows.GetArray())
	{
		Artwork art;
		uint64_t width, height;
		if (!number(row, "id", art.id) || !number(row, "hearts", art.hearts) ||
			!number(row, "width", width) || !number(row, "height", height) ||
			!width || !height || width > 100000 || height > 100000 || !row.HasMember("url") || !row["url"].IsString() ||
			!row.HasMember("mime") || !row["mime"].IsString())
		{
			fail(Error::InvalidResponse, 500);
			return;
		}
		if (row.HasMember("score") && row["score"].IsInt64()) art.score = row["score"].GetInt64();
		const std::string mime = row["mime"].GetString();
		art.url = row["url"].GetString();
		if (row.HasMember("is_animated") && row["is_animated"].IsBool() && row["is_animated"].GetBool()) continue;
		if (art.url.compare(0, 8, "https://") != 0 || (mime != "image/png" && (mArtwork == 2 || mime != "image/jpeg"))) continue;
		if (mArtwork == 0 && mVerticalImage && (width != 600 || height != 900)) continue;
		if (mArtwork == 0 && !mVerticalImage && !((width == 460 && height == 215) || (width == 920 && height == 430))) continue;
		if (mArtwork == 1 && !((width == 512 && height == 512) || (width == 1024 && height == 1024))) continue;
		art.pixels = width * height;
		art.extension = mime == "image/png" ? ".png" : ".jpg";
		auto& best = mGames[mGame].artwork[mArtwork];
		if (better(art, best, mPreferScore)) best = art;
	}
	mSeen += rows.Size();
	uint64_t total = 0, page = 0;
	const auto& data = doc["data"];
	if (!number(data, "current_page", page) || page != mPage)
	{
		fail(Error::InvalidResponse, 500);
		return;
	}
	if (!number(data, "total", total))
	{
		fail(Error::InvalidResponse, 500);
		return;
	}
	const bool more = mSeen < total;
	if (more)
	{
		// Bound a broken/infinite pagination response without choosing a winner
		// from incomplete data. Normal game collections are far below this size.
		if (rows.Empty() || ++mPage >= 200) { fail(Error::InvalidResponse, 500); return; }
	}
	else nextArtwork();
}

} // namespace SteamGridDB
