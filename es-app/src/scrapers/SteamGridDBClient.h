#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// API/search logic is independent of the frontend and HTTP transport so that
// pagination, title matching and failure handling can be exercised locally.
namespace SteamGridDB
{

enum class Error { None, MissingKey, InvalidKey, Network, RateLimited, ServiceUnavailable, InvalidResponse };

struct Artwork
{
	uint64_t id = 0;
	uint64_t hearts = 0;
	int64_t score = 0;
	uint64_t pixels = 0;
	std::string url;
	std::string extension;
};

struct Game
{
	uint64_t id = 0;
	std::string name;
	double relevance = 0;
	// Steam horizontal grid for image, square grid for thumbnail, logo.
	std::array<Artwork, 3> artwork;
};

std::vector<std::string> searchNames(const std::string& name, bool explicitName);
double titleRelevance(const std::string& query, const std::string& title);
Error checkKey(const std::string& trimmedKey);

class Client
{
public:
	Client(const std::string& name, bool explicitName, bool manual, const std::array<bool, 3>& enabled, bool verticalImage = false, bool preferScore = false);

	std::string path() const;
	std::string body() const; // Empty for authenticated GET search; JSON for artwork search.
	void receive(int status, const std::string& body);
	bool done() const { return mDone; }
	Error error() const { return mError; }
	int errorCode() const { return mErrorCode; }
	const std::vector<Game>& results() const { return mGames; }

private:
	void finishSearch();
	void nextArtwork();
	void fail(Error error, int status);

	std::vector<std::string> mQueries;
	size_t mQuery = 0;
	bool mSearching = true;
	bool mManual;
	bool mVerticalImage;
	bool mPreferScore;
	std::array<bool, 3> mEnabled;
	std::vector<Game> mGames;
	size_t mGame = 0;
	size_t mArtwork = 0;
	unsigned int mPage = 0;
	uint64_t mSeen = 0;
	bool mDone = false;
	Error mError = Error::None;
	int mErrorCode = 200;
};

}
