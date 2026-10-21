#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Shared by scraper queries and title matching. Only useful transformations are
// generated; regional names must come from metadata or a provider's records.
namespace GameSearchNames
{
constexpr size_t MaxFilenameCandidates = 3;
constexpr size_t MaxCandidates = 8;
std::string normalized(const std::string& value);
std::vector<std::string> candidates(const std::string& name, bool explicitName = false,
	const std::vector<std::string>& knownNames = {});
double relevance(const std::string& query, const std::string& title);
}
