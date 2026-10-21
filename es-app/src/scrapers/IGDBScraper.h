#pragma once

#ifndef ES_APP_SCRAPERS_IGDBDB_SCRAPER_H
#define ES_APP_SCRAPERS_IGDBDB_SCRAPER_H

#include "scrapers/Scraper.h"
#include "utils/TimeUtil.h"

namespace pugi
{
	class xml_document;
}

class IGDBScraper : public Scraper
{
public:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;

	bool isSupportedPlatform(SystemData* system) override;
	bool hasMissingMedia(FileData* file) override;

	const std::set<ScraperMediaSource>& getSupportedMedias() override;

private:
	bool ensureToken();

	std::string mToken;
	Utils::Time::DateTime mTokenDate;
};

class IGDBRequest : public ScraperHttpRequest
{
public:
	// Search request with an optional filename fallback and parent media lookup.
	IGDBRequest(std::queue<std::unique_ptr<ScraperRequest>>& requestsWrite, std::vector<ScraperSearchResult>& resultsWrite, const std::string& url, HttpReqOptions* options, bool isArcade, bool isManualScrape, const std::string& fallbackQuery = "")
		: ScraperHttpRequest(resultsWrite, url, options), mRequestQueue(requestsWrite), mRequestOptions(*options), mFallbackQuery(fallbackQuery)
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
	std::string mFallbackQuery;
};

class IGDBParentMediaRequest : public ScraperHttpRequest
{
public:
	typedef std::map<unsigned int, std::vector<size_t>> ParentResults;

	IGDBParentMediaRequest(std::vector<ScraperSearchResult>& results, const std::string& url, HttpReqOptions* options,
		std::vector<ScraperSearchResult> pendingResults, ParentResults parentResults, bool isManualScrape);

	void update() override;
	bool retryOn249() override { return !mIsManualScrape; }

protected:
	bool process(const std::string& response, std::vector<ScraperSearchResult>& results) override;

private:
	std::vector<ScraperSearchResult> mPendingResults;
	ParentResults mParentResults;
	bool mIsManualScrape;
};

#endif // ES_APP_SCRAPERS_IGDBDB_SCRAPER_H
