#pragma once

#include "scrapers/Scraper.h"

class SteamGridDBScraper : public Scraper
{
public:
	bool isSupportedPlatform(SystemData*) override { return true; }
	const std::set<ScraperMediaSource>& getSupportedMedias() override;
	int getThreadCount(std::string& error) override;
	bool hasAnyMedia(FileData* file) override;
	bool hasMissingMedia(FileData* file) override;

protected:
	void generateRequests(const ScraperSearchParams& params,
		std::queue<std::unique_ptr<ScraperRequest>>& requests,
		std::vector<ScraperSearchResult>& results) override;
};
