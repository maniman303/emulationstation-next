#include "scrapers/IGDBSearch.h"
#include "scrapers/IGDBMetadata.h"
#include <cstdlib>
#include <iostream>

static void check(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	const auto rhythm = IGDBSearch::queries("fields id, name;", "3699 - Rhythm Paradise (EU)(M3)(EXiMiUS)", "", "20");
	check(rhythm.size() == 2, "numbered ROM search has bounded fallbacks");
	check(rhythm[0].find("search \"Rhythm Paradise\"") != std::string::npos, "release tags and catalogue prefix removed first");
	check(rhythm[1] == "fields id, name; where (name ~ \"Rhythm Paradise\" | alternative_names.name ~ \"Rhythm Paradise\" | game_localizations.name ~ \"Rhythm Paradise\") & platforms = (20); limit 10;",
		"regional title explicitly matched against aliases and localizations on Nintendo DS");
	for (const auto& query : rhythm)
	{
		check(query.find("platforms = (20)") != std::string::npos, "every retry retains platform restriction");
		check(query.find("release_dates.platform") == std::string::npos, "games without release-date records remain searchable");
	}
	for (const auto& stem : {
		"5687 - LEGO Pirates of the Caribbean - The Video Game (Europe) (En,Fr,De,Es,It,Nl,Sv,No,Da)",
		"2339 - LEGO Indiana Jones - The Original Adventures (Europe) (En,Fr,De,Es,It,Da)"})
	{
		const auto lego = IGDBSearch::queries("fields id;", stem, "", "20");
		check(lego.size() == 2 && lego[0].find("search \"LEGO ") != std::string::npos, "LEGO filenames get clean title retry");
	}
	const auto numeric = IGDBSearch::queries("fields id;", "1942 - Joint Strike", "", "9");
	check(numeric.front().find("search \"1942 - Joint Strike\"") != std::string::npos, "meaningful numeric title searched first");
	const auto manual = IGDBSearch::queries("fields id;", "ignored", "3699 - Rhythm Paradise (EU)", "20");
	check(manual.size() == 2 && manual.back().find("alternative_names.name ~ \"3699 - Rhythm Paradise (EU)\"") != std::string::npos,
		"explicit search text preserved for both title and alias lookup");
	const auto unfiltered = IGDBSearch::queries("fields id;", "Rhythm Paradise", "", "");
	check(unfiltered.size() == 2 && unfiltered.back().find("platforms") == std::string::npos, "unknown platform has valid unfiltered alias query");
	check(IGDBSearch::quote("A \"Title\" \\ Test") == "\"A \\\"Title\\\" \\\\ Test\"", "query quotes and backslashes escaped");
	check(IGDBSearch::queries("fields id;", "(EU)", "", "20").empty(), "empty title does not query the entire database");

	rapidjson::Document parent;
	parent.Parse(R"({"involved_companies":[
		{"developer":true,"company":{"name":"Traveller's Tales"}},
		{"publisher":true,"developer":false,"company":{"name":"LucasArts"}}
	]})");
	check(IGDBMetadata::developer(parent) == "Traveller's Tales", "missing developer inherited from parent's developer company");
	check(IGDBMetadata::developer(parent, "TT Fusion") == "TT Fusion", "child developer preserved when parent differs");
	parent.Parse(R"({"involved_companies":[{"publisher":true,"company":{"name":"LucasArts"}}]})");
	check(IGDBMetadata::developer(parent).empty(), "publisher is never used as developer");
	parent.Parse(R"({"involved_companies":[null,{},
		{"developer":"true","company":{"name":"Invalid flag"}},
		{"developer":true,"company":null},
		{"developer":true,"company":{"name":123}},
		{"developer":true,"company":{"name":""}}
	]})");
	check(IGDBMetadata::developer(parent).empty(), "malformed or empty company data ignored");
	parent.Parse("{}");
	check(IGDBMetadata::developer(parent).empty(), "missing parent companies leave developer empty");
	check(IGDBMetadata::developer(parent, "TT Fusion") == "TT Fusion", "missing parent data preserves existing developer");
	parent.Parse(R"({"involved_companies":[
		{"developer":true,"company":{"name":"Traveller's Tales"}},
		{"developer":true,"company":{"name":"TransGaming Inc."}},
		{"developer":true,"company":{"name":"Traveller's Tales"}}
	]})");
	check(IGDBMetadata::developer(parent) == "Traveller's Tales, TransGaming Inc.", "all distinct parent developers retained");
	parent.Parse(R"({"name":"Parent title","summary":"Parent description","rating":82,
		"involved_companies":[{"developer":true,"company":{"name":"Parent studio"}},
			{"publisher":true,"company":{"name":"Parent publisher"}}],
		"genres":[{"name":"Adventure"},{"name":"Puzzle"}],
		"release_dates":[{"platform":6,"date":100},{"platform":20,"date":200}],
		"multiplayer_modes":[{"platform":6,"offlinemax":8},{"platform":20,"offlinemax":2}]
	})");
	const auto parentInfo = IGDBMetadata::read(parent, "20");
	const auto filled = IGDBMetadata::inherit({}, parentInfo);
	check(filled.description == "Parent description" && filled.developer == "Parent studio" && filled.publisher == "Parent publisher",
		"general fallback fills missing description and both company roles");
	check(filled.genre == "Adventure, Puzzle" && filled.players == 2 && filled.rating == .82 && filled.releaseDate == 200,
		"general fallback fills genre, players, rating and platform-specific date");
	check(IGDBMetadata::missingFields(filled).empty(), "complete metadata needs no parent lookup");

	rapidjson::Document child;
	child.Parse(R"({"summary":"DS description","aggregated_rating":60,
		"involved_companies":[{"developer":true,"publisher":true,"company":{"name":"DS studio"}}],
		"genres":[{"name":"Music"}],"release_dates":[{"platform":20,"date":300}],
		"game_modes":[{"name":"Single player"}]
	})");
	const auto own = IGDBMetadata::inherit(IGDBMetadata::read(child, "20"), parentInfo);
	check(own.description == "DS description" && own.developer == "DS studio" && own.publisher == "DS studio" &&
		own.genre == "Music" && own.players == 1 && own.rating == .6 && own.releaseDate == 300,
		"every child field wins over parent, including aggregate rating and port release date");
	child.Parse(R"({"rating":0,"involved_companies":null,"genres":[null,{}],"release_dates":[{},null]})");
	const auto partial = IGDBMetadata::read(child, "20");
	check(IGDBMetadata::inherit(partial, parentInfo).rating == 0, "valid zero rating preserved");
	const auto needed = IGDBMetadata::missingFields(partial);
	check(needed.count("summary") && needed.count("involved_companies.publisher") && needed.count("genres.name") &&
		needed.count("release_dates.platform") && needed.count("multiplayer_modes.platform") && !needed.count("rating"),
		"parent request selects exactly missing metadata groups");
	const auto noParentData = IGDBMetadata::inherit(partial, {});
	check(noParentData.description.empty() && noParentData.developer.empty() && noParentData.rating == 0 && !noParentData.releaseDate,
		"unavailable parent data leaves child values intact without fabricated metadata");
	std::cout << "IGDB search and parent metadata regression checks passed\n";
}
