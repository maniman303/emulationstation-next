#pragma once

#ifndef ES_APP_SCRAPERS_IGDBDB_SCRAPER_H
#define ES_APP_SCRAPERS_IGDBDB_SCRAPER_H

#include "scrapers/Scraper.h"
#include "scrapers/IGDBMetadata.h"
#include "utils/TimeUtil.h"
#include <chrono>
#include <mutex>

namespace pugi
{
	class xml_document;
}

// Optional, deferred canonical-title lookup shared with other scraper transports.
// HTTP starts only on update(), so ordinary successful searches need no lookup.
class IGDBNameLookup : public AsyncHandle
{
public:
	IGDBNameLookup(const HttpReqOptions& options) : mOptions(options) { setStatus(ASYNC_IN_PROGRESS); }
	void update() override;
	const std::vector<std::string>& names() const { return mNames; }
private:
	HttpReqOptions mOptions;
	std::unique_ptr<HttpReq> mRequest;
	std::chrono::steady_clock::time_point mStarted;
	std::vector<std::string> mNames;
};

class IGDBScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	std::unique_ptr<IGDBNameLookup> prepareNameLookup(const ScraperSearchParams& params);
	bool isSupportedPlatform(SystemData* system) override;
	bool hasMissingMedia(FileData* file) override;

	const std::set<ScraperMediaSource>& getSupportedMedias() override;

private:
	bool ensureToken();

	std::mutex mTokenMutex;
	std::string mToken;
	Utils::Time::DateTime mTokenDate;
};

class IGDBRequest : public ScraperHttpRequest
{
public:
	// Search request with bounded title fallbacks and parent metadata and media lookup.
	IGDBRequest(std::queue<std::unique_ptr<ScraperRequest>>& requestsWrite, std::vector<ScraperSearchResult>& resultsWrite, const std::string& url, HttpReqOptions* options, bool isArcade, bool isManualScrape, const std::vector<std::string>& fallbackQueries = {}, const std::string& preferredPlatform = "")
		: ScraperHttpRequest(resultsWrite, url, options), mRequestQueue(requestsWrite), mRequestOptions(*options), mFallbackQueries(fallbackQueries), mPreferredPlatform(preferredPlatform)
	{
		mIsManualScrape = isManualScrape;
		mIsArcade = isArcade;
	}

	virtual bool retryOn249() { return !mIsManualScrape; }

protected:
	bool process(const std::string& response, std::vector<ScraperSearchResult>& results) override;

	bool mIsManualScrape;
	bool mIsArcade;
	std::queue<std::unique_ptr<ScraperRequest>>& mRequestQueue;
	HttpReqOptions mRequestOptions;
	std::vector<std::string> mFallbackQueries;
	std::string mPreferredPlatform;
};

class IGDBParentRequest : public ScraperHttpRequest
{
public:
	struct ParentResult
	{
		size_t index;
		bool inheritMedia;
		IGDBMetadata::Info info;
	};
	typedef std::map<unsigned int, std::vector<ParentResult>> ParentResults;

	IGDBParentRequest(std::vector<ScraperSearchResult>& results, const std::string& url, HttpReqOptions* options,
		std::vector<ScraperSearchResult> pendingResults, ParentResults parentResults, bool isManualScrape, const std::string& preferredPlatform);

	void update() override;
	bool retryOn249() override { return !mIsManualScrape; }

protected:
	bool process(const std::string& response, std::vector<ScraperSearchResult>& results) override;

private:
	std::vector<ScraperSearchResult> mPendingResults;
	ParentResults mParentResults;
	bool mIsManualScrape;
	std::string mPreferredPlatform;
};

#endif // ES_APP_SCRAPERS_IGDBDB_SCRAPER_H
