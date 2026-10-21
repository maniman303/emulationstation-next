#pragma once

#include <rapidjson/document.h>
#include <string>
#include <cstdint>
#include <set>
#include <algorithm>

namespace IGDBMetadata
{

inline std::string company(const rapidjson::Value& game, const char* role, const std::string& current = "")
{
	if (!current.empty())
		return current;
	if (!game.IsObject() || !game.HasMember("involved_companies") || !game["involved_companies"].IsArray())
		return "";

	std::string result;
	std::set<std::string> names;
	for (const auto& involvement : game["involved_companies"].GetArray())
	{
		if (!involvement.IsObject() || !involvement.HasMember(role) ||
			!involvement[role].IsBool() || !involvement[role].GetBool() ||
			!involvement.HasMember("company") || !involvement["company"].IsObject())
			continue;
		const auto& company = involvement["company"];
		if (company.HasMember("name") && company["name"].IsString() && company["name"].GetStringLength() > 0 &&
			names.insert(company["name"].GetString()).second)
			result += (result.empty() ? "" : ", ") + std::string(company["name"].GetString());
	}
	return result;
}

inline std::string developer(const rapidjson::Value& game, const std::string& current = "")
{
	return company(game, "developer", current);
}

struct Info
{
	std::string description;
	std::string developer;
	std::string publisher;
	std::string genre;
	int players = 0;
	double rating = -1;
	uint64_t releaseDate = 0;
};

inline Info read(const rapidjson::Value& game, const std::string& platform)
{
	Info info;
	if (!game.IsObject())
		return info;
	if (game.HasMember("summary") && game["summary"].IsString())
		info.description = game["summary"].GetString();
	info.developer = company(game, "developer");
	info.publisher = company(game, "publisher");
	for (const char* field : {"rating", "aggregated_rating"})
	{
		if (game.HasMember(field) && game[field].IsNumber() && game[field].GetDouble() >= 0 && game[field].GetDouble() <= 100)
		{
			info.rating = game[field].GetDouble() / 100.0;
			break;
		}
	}
	if (game.HasMember("genres") && game["genres"].IsArray())
		for (const auto& genre : game["genres"].GetArray())
			if (genre.IsObject() && genre.HasMember("name") && genre["name"].IsString() && genre["name"].GetStringLength() > 0)
				info.genre += (info.genre.empty() ? "" : ", ") + std::string(genre["name"].GetString());

	uint64_t fallbackDate = 0;
	if (game.HasMember("release_dates") && game["release_dates"].IsArray())
		for (const auto& release : game["release_dates"].GetArray())
		{
			if (!release.IsObject() || !release.HasMember("date") || !release["date"].IsUint64() || release["date"].GetUint64() == 0)
				continue;
			const auto date = release["date"].GetUint64();
			if (!fallbackDate || date < fallbackDate)
				fallbackDate = date;
			if (!platform.empty() && release.HasMember("platform") && release["platform"].IsUint() &&
				std::to_string(release["platform"].GetUint()) == platform && (!info.releaseDate || date < info.releaseDate))
				info.releaseDate = date;
		}
	if (!info.releaseDate)
		info.releaseDate = fallbackDate;

	if (game.HasMember("multiplayer_modes") && game["multiplayer_modes"].IsArray())
		for (const auto& mode : game["multiplayer_modes"].GetArray())
		{
			if (!mode.IsObject() || !mode.HasMember("offlinemax") || !mode["offlinemax"].IsInt())
				continue;
			if (!platform.empty() && mode.HasMember("platform") && mode["platform"].IsUint() &&
				std::to_string(mode["platform"].GetUint()) != platform)
				continue;
			info.players = std::max(info.players, mode["offlinemax"].GetInt());
		}
	if (!info.players && game.HasMember("game_modes") && game["game_modes"].IsArray())
		for (const auto& mode : game["game_modes"].GetArray())
			if (mode.IsObject() && mode.HasMember("name") && mode["name"].IsString() && std::string(mode["name"].GetString()) == "Single player")
				info.players = 1;
	return info;
}

inline std::set<std::string> missingFields(const Info& info)
{
	std::set<std::string> fields;
	if (info.description.empty()) fields.insert("summary");
	if (info.developer.empty() || info.publisher.empty())
	{
		fields.insert("involved_companies.company.name");
		fields.insert("involved_companies.developer");
		fields.insert("involved_companies.publisher");
	}
	if (info.genre.empty()) fields.insert("genres.name");
	if (!info.players)
	{
		fields.insert("game_modes.name");
		fields.insert("multiplayer_modes.offlinemax");
		fields.insert("multiplayer_modes.platform");
	}
	if (info.rating < 0)
	{
		fields.insert("rating");
		fields.insert("aggregated_rating");
	}
	if (!info.releaseDate)
	{
		fields.insert("release_dates.date");
		fields.insert("release_dates.platform");
	}
	return fields;
}

inline Info inherit(Info child, const Info& parent)
{
	if (child.description.empty()) child.description = parent.description;
	if (child.developer.empty()) child.developer = parent.developer;
	if (child.publisher.empty()) child.publisher = parent.publisher;
	if (child.genre.empty()) child.genre = parent.genre;
	if (!child.players) child.players = parent.players;
	if (child.rating < 0) child.rating = parent.rating;
	if (!child.releaseDate) child.releaseDate = parent.releaseDate;
	return child;
}

}
