#include "scrapers/SteamGridDBClient.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>
#include <rapidjson/document.h>

namespace SteamGridDB
{

static std::string compactWhitespace(const std::string& value)
{
	std::istringstream input(value);
	std::string word, result;
	while (input >> word)
	{
		if (!result.empty()) result += ' ';
		result += word;
	}
	return result;
}

static std::string normalized(const std::string& value)
{
	std::string result;
	for (size_t i = 0; i < value.size(); ++i)
	{
		unsigned char c = value[i];
		// Latin accents occur in many ROM names (notably Pokémon). Keep other
		// UTF-8 text intact rather than discarding non-English titles.
		if (c == 0xc3 && i + 1 < value.size())
		{
			unsigned char next = value[++i];
			if ((next >= 0x80 && next <= 0x85) || (next >= 0xa0 && next <= 0xa5)) result += 'a';
			else if (next == 0x87 || next == 0xa7) result += 'c';
			else if ((next >= 0x88 && next <= 0x8b) || (next >= 0xa8 && next <= 0xab)) result += 'e';
			else if ((next >= 0x8c && next <= 0x8f) || (next >= 0xac && next <= 0xaf)) result += 'i';
			else if (next == 0x91 || next == 0xb1) result += 'n';
			else if ((next >= 0x92 && next <= 0x96) || (next >= 0xb2 && next <= 0xb6) || next == 0x98 || next == 0xb8) result += 'o';
			else if ((next >= 0x99 && next <= 0x9c) || (next >= 0xb9 && next <= 0xbc)) result += 'u';
			else if (next == 0x9d || next == 0xbd || next == 0xbf) result += 'y';
			else { result += static_cast<char>(c); result += static_cast<char>(next); }
		}
		else if (c == '\'') continue;
		else if (c < 0x80) result += std::isalnum(c) ? static_cast<char>(std::tolower(c)) : ' ';
		else result += static_cast<char>(c);
	}
	return compactWhitespace(result);
}

static std::set<std::string> words(const std::string& text)
{
	std::istringstream input(text);
	std::set<std::string> result;
	std::string word;
	while (input >> word) result.insert(word);
	return result;
}

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

static bool better(const Artwork& a, const Artwork& b)
{
	if (b.url.empty()) return true;
	if (a.hearts != b.hearts) return a.hearts > b.hearts;
	// Upvotes, scores and API ordering have no influence, even on ties.
	if (a.pixels != b.pixels) return a.pixels > b.pixels;
	return a.id < b.id;
}

std::vector<std::string> searchNames(const std::string& name, bool explicitName)
{
	std::string full = compactWhitespace(name);
	if (full.empty()) return {};
	if (explicitName) return {full}; // Respect the user's manual search text.

	static const std::regex tags(R"(\([^()]*\)|\[[^\[\]]*\])");
	std::string clean = full;
	// Nested release tags are uncommon, but bounded passes handle them too.
	for (int i = 0; i < 4; ++i) clean = std::regex_replace(clean, tags, " ");
	for (auto& c : clean) if (c == '_' || c == '.') c = ' ';
	clean = compactWhitespace(clean);
	static const std::regex catalogue(R"(^([0-9]{3,6})(?:\s*[-.]\s*|\s+)(.+)$)");
	std::smatch prefix;
	std::string withoutPrefix = clean;
	bool likelyCatalogue = false;
	if (std::regex_match(clean, prefix, catalogue))
	{
		withoutPrefix = prefix[2].str();
		// A real title such as "1942 - Joint Strike" remains the first query
		// unless release tags or a zero-padded/long index indicate a ROM set.
		const auto index = std::stoi(prefix[1].str());
		likelyCatalogue = prefix[1].str()[0] == '0' || prefix[1].length() > 4 ||
			((index < 1900 || index > 2099) && full != std::regex_replace(full, tags, " "));
	}
	static const std::regex article(R"(^(.+),\s*(The|A|An)$)", std::regex::icase);
	clean = std::regex_replace(clean, article, "$2 $1");
	withoutPrefix = std::regex_replace(withoutPrefix, article, "$2 $1");

	std::vector<std::string> result;
	std::set<std::string> seen;
	auto add = [&](const std::string& value)
	{
		const std::string key = normalized(value);
		if (!key.empty() && seen.insert(key).second && result.size() < 3) result.push_back(value);
	};
	add(likelyCatalogue ? withoutPrefix : clean);
	add(likelyCatalogue ? clean : withoutPrefix);
	static const std::regex version(R"(\s+Version$)", std::regex::icase);
	add(std::regex_replace(withoutPrefix, version, ""));
	add(full);
	return result;
}

double titleRelevance(const std::string& query, const std::string& title)
{
	auto a = normalized(query), b = normalized(title);
	auto removeVersion = [](std::string& text)
	{
		const std::string suffix = " version";
		if (text.size() > suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0)
			text.erase(text.size() - suffix.size());
	};
	removeVersion(a);
	removeVersion(b);
	if (a.empty() || b.empty()) return 0;
	if (a == b) return 1;
	auto joinedA = a, joinedB = b;
	joinedA.erase(std::remove(joinedA.begin(), joinedA.end(), ' '), joinedA.end());
	joinedB.erase(std::remove(joinedB.begin(), joinedB.end(), ' '), joinedB.end());
	if (joinedA == joinedB) return 1;
	const auto aw = words(a), bw = words(b);
	// Never silently turn a numbered game into a different sequel.
	std::set<std::string> an, bn;
	for (auto& word : aw) if (std::all_of(word.begin(), word.end(), [](unsigned char c) { return std::isdigit(c); })) an.insert(word);
	for (auto& word : bw) if (std::all_of(word.begin(), word.end(), [](unsigned char c) { return std::isdigit(c); })) bn.insert(word);
	if (an != bn) return 0;
	size_t common = 0;
	for (auto& word : aw) if (bw.count(word)) ++common;
	return static_cast<double>(common) / std::max(aw.size(), bw.size());
}

Error checkKey(const std::string& trimmedKey)
{
	if (trimmedKey.empty()) return Error::MissingKey;
	for (unsigned char c : trimmedKey)
		if (c <= 0x20 || c >= 0x7f) return Error::InvalidKey;
	return Error::None; // Key length/format is validated by the API.
}

Client::Client(const std::string& name, bool explicitName, bool manual, const std::array<bool, 3>& enabled)
	: mQueries(searchNames(name, explicitName)), mManual(manual), mEnabled(enabled)
{
	if (mQueries.empty()) mDone = true;
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
	if (mArtwork == 0) body += "\"460x215\",\"920x430\"";
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
		const std::string mime = row["mime"].GetString();
		art.url = row["url"].GetString();
		if (row.HasMember("is_animated") && row["is_animated"].IsBool() && row["is_animated"].GetBool()) continue;
		if (art.url.compare(0, 8, "https://") != 0 || (mime != "image/png" && mime != "image/jpeg")) continue;
		if (mArtwork == 0 && !((width == 460 && height == 215) || (width == 920 && height == 430))) continue;
		if (mArtwork == 1 && !((width == 512 && height == 512) || (width == 1024 && height == 1024))) continue;
		art.pixels = width * height;
		art.extension = mime == "image/png" ? ".png" : ".jpg";
		auto& best = mGames[mGame].artwork[mArtwork];
		if (better(art, best)) best = art;
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
