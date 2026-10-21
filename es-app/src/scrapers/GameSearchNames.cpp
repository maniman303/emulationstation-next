#include "scrapers/GameSearchNames.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

namespace GameSearchNames
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

std::string normalized(const std::string& value)
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

std::vector<std::string> candidates(const std::string& name, bool explicitName, const std::vector<std::string>& knownNames)
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
	bool meaningfulNumber = false;
	if (std::regex_match(clean, prefix, catalogue))
	{
		withoutPrefix = prefix[2].str();
		// A real title such as "1942 - Joint Strike" remains the first query
		// unless release tags or a zero-padded/long index indicate a ROM set.
		const auto index = std::stoi(prefix[1].str());
		meaningfulNumber = index >= 1900 && index <= 2099;
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
		if (!key.empty() && seen.insert(key).second && result.size() < MaxFilenameCandidates) result.push_back(value);
	};
	add(likelyCatalogue ? withoutPrefix : clean);
	// Preserve meaningful year/numeric titles rather than searching a different game.
	if (!likelyCatalogue && meaningfulNumber)
		withoutPrefix = clean;
	if (!likelyCatalogue) add(withoutPrefix);
	static const std::regex version(R"(\s+Version$)", std::regex::icase);
	add(std::regex_replace(withoutPrefix, version, ""));
	// Tagged ROM filenames add no useful title information to a remote search.
	for (const auto& known : knownNames)
	{
		if (result.size() >= MaxCandidates) break;
		for (const auto& variant : candidates(known, false, {}))
		{
			const auto key = normalized(variant);
			if (!key.empty() && seen.insert(key).second && result.size() < MaxCandidates)
				result.push_back(variant);
		}
	}
	return result;
}

double relevance(const std::string& query, const std::string& title)
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

}
