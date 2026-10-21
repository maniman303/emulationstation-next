#pragma once

#include "scrapers/GameSearchNames.h"
#include <rapidjson/document.h>
#include <string>
#include <vector>

namespace IGDBSearch
{

inline std::string quote(const std::string& value)
{
	std::string result = "\"";
	for (char c : value)
	{
		if (c == '\\' || c == '"')
			result += '\\';
		result += c;
	}
	return result + "\"";
}

inline std::string aliasQuery(const std::string& fields, const std::vector<std::string>& names, const std::string& platform)
{
	if (names.empty()) return "";
	std::string condition;
	for (const auto& name : names)
	{
		if (!condition.empty()) condition += " | ";
		condition += "name ~ " + quote(name) + " | alternative_names.name ~ " + quote(name) +
			" | game_localizations.name ~ " + quote(name);
	}
	return fields + " where (" + condition + ")" +
		(platform.empty() ? "" : " & platforms = (" + platform + ")") + "; limit 10;";
}

inline std::vector<std::string> queries(const std::string& fields, const std::string& stem,
	const std::string& nameOverride, const std::string& platform, const std::vector<std::string>& knownNames = {})
{
	const auto names = GameSearchNames::candidates(nameOverride.empty() ? stem : nameOverride, !nameOverride.empty(), knownNames);
	std::vector<std::string> result;
	for (const auto& name : names)
		result.push_back(fields + " search " + quote(name) + "; limit 10;" +
			(platform.empty() ? "" : " where platforms = (" + platform + ");"));
	const auto aliases = aliasQuery(fields, names, platform);
	if (!aliases.empty()) result.push_back(aliases);
	return result;
}

// The exact alias/localization query establishes the identity of these records.
// Return canonical titles rather than inventing regional word substitutions.
inline std::vector<std::string> confirmedNames(const std::string& response)
{
	rapidjson::Document doc;
	doc.Parse(response.c_str());
	std::vector<std::string> names;
	if (!doc.IsArray()) return names;
	for (const auto& game : doc.GetArray())
		if (game.IsObject() && game.HasMember("name") && game["name"].IsString() && names.size() < 5)
			names.push_back(game["name"].GetString());
	return names;
}

}
