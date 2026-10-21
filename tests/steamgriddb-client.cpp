#include "scrapers/SteamGridDBClient.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

using namespace SteamGridDB;
static int checks = 0;

static void check(bool condition, const char* message)
{
	++checks;
	if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

static std::string page(const std::string& rows, unsigned int total, unsigned int index = 0)
{
	return "{\"success\":true,\"data\":{\"current_page\":" + std::to_string(index) + ",\"total\":" + std::to_string(total) + ",\"assets\":[" + rows + "]}}";
}

static std::string art(int id, int hearts, int score, int width = 600, int height = 900)
{
	return "{\"id\":" + std::to_string(id) + ",\"hearts\":" + std::to_string(hearts) + ",\"upvotes\":" + std::to_string(std::max(0, score)) +
		",\"score\":" + std::to_string(score) + ",\"width\":" + std::to_string(width) + ",\"height\":" + std::to_string(height) +
		",\"url\":\"https://cdn2.steamgriddb.com/grid/" + std::to_string(id) + ".png\",\"mime\":\"image/png\"}";
}

static const std::string match = R"({"success":true,"data":[{"id":37371,"name":"Metroid Fusion"}]})";

static void tests()
{
	check(checkKey("") == Error::MissingKey, "missing key rejected locally");
	check(checkKey("bad\r\nHeader: injected") == Error::InvalidKey, "header injection rejected");
	check(checkKey(std::string(31, 'a')) == Error::None, "do not assume a 32-character API key");
	check(searchNames("   ", false).empty(), "empty query sends no requests");
	check(searchNames("Metroid Fusion", false).size() == 1, "ordinary title has no redundant variants");
	check(searchNames("Pokemon Emerald Version", false).size() == 2, "generic Version suffix gets one bounded fallback query");
	const auto variants = searchNames("1633 - Metroid_Fusion (USA) [! weirdo]", false);
	check(variants.front() == "Metroid Fusion" && variants.size() == 3, "numbered tagged filename cleaned first with bounded variants");
	check(searchNames("1942 - Joint Strike", false).front() == "1942 - Joint Strike", "preserve meaningful numeric title");
	check(searchNames("0001 - Electroplankton", false).front() == "Electroplankton", "zero-padded catalogue prefix");
	check(searchNames("0001_Electroplankton (Japan)", false).front() == "Electroplankton", "catalogue prefix with underscore separator");
	check(searchNames("0001-Electroplankton", false).front() == "Electroplankton", "catalogue prefix without spaces");
	check(searchNames("2020 Super Baseball (USA)", false).front() == "2020 Super Baseball", "year in a real title preserved even with release tags");
	check(searchNames("Legend of Zelda, The (USA)", false).front() == "The Legend of Zelda", "trailing article moved to front");
	check(searchNames("1633 - Title (USA)", true) == std::vector<std::string>{"1633 - Title (USA)"}, "explicit search respected");
	check(titleRelevance("Pokemon Emerald", "Pokémon Emerald") == 1, "accent-insensitive title matching");
	check(titleRelevance("Pokemon - Emerald Version", "Pokémon Emerald") == 1, "generic ROM version suffix does not hide a correct title");
	check(titleRelevance("Pokemon FireRed Version", "Pokémon Fire Red") == 1, "joined title words match without changing sequel numbers");
	check(titleRelevance("Super Mario Land 2", "Super Mario Land 3") == 0, "do not confuse numbered sequels");
	check(titleRelevance("Metroid Fusion", "Metroid") < .75, "unrelated fuzzy autocomplete result rejected for automatic scrape");
	check(titleRelevance("Mario & Luigi", "Mario Luigi") > .95, "punctuation normalized");

	Client client("1633 - Metroid Fusion (USA)", false, false, {{true, true, true}});
	check(client.path() == "/api/v2/search/autocomplete/Metroid%20Fusion", "query is cleaned and URL encoded");
	client.receive(200, match);
	check(client.body().find("460x215\",\"920x430") != std::string::npos, "exact match immediately fetches horizontal grid for image");
	client.receive(200, page(art(1, 2, 999, 460, 215), 2));
	check(client.body().find("\"page\":1") != std::string::npos, "follow pagination even when first page is short");
	client.receive(200, page(art(2, 8, -100, 920, 430), 2, 1));
	check(client.body().find("512x512\",\"1024x1024") != std::string::npos, "square grid follows horizontal image");
	client.receive(200, page(art(3, 7, 1000, 512, 512) + "," + art(4, 9, -20, 1024, 1024), 2));
	check(client.body().find("\"asset_type\":\"logo\"") != std::string::npos && client.body().find("image/jpeg") == std::string::npos, "logo endpoint accepts only PNG mime");
	client.receive(200, page(art(5, 11, 0, 1500, 500), 1));
	check(client.done() && client.error() == Error::None, "successful scrape completes");
	check(client.results().size() == 1 && client.results()[0].name == "Metroid Fusion", "title returned");
	check(client.results()[0].artwork[0].id == 2, "horizontal image with highest hearts across pages wins over score and upvotes");
	check(client.results()[0].artwork[1].id == 4, "square thumbnail selected by hearts");
	check(client.results()[0].artwork[2].id == 5, "logo selected by hearts");
	check(client.path().empty(), "completed client has no request");
	Client encoded("A/B?C & D", true, false, {{false, false, false}});
	check(encoded.path() == "/api/v2/search/autocomplete/A%2FB%3FC%20%26%20D", "reserved characters cannot change API path or query");
	std::string suggestions;
	for (int i = 0; i < 10; ++i)
	{
		if (i) suggestions += ',';
		suggestions += "{\"id\":" + std::to_string(i + 1) + ",\"name\":\"Metroid Fusion" + (i == 9 ? "" : " Deluxe") + "\"}";
	}
	Client manual("Metroid Fusion", false, true, {{false, false, false}});
	manual.receive(200, "{\"success\":true,\"data\":[" + suggestions + "]}");
	check(manual.done() && manual.results().size() == 5, "manual search bounds artwork work to five games");
	check(manual.results()[0].id == 10, "exact title outranks API fuzzy ordering");
	Client automatic("Metroid Fusion", false, false, {{false, false, false}});
	automatic.receive(200, "{\"success\":true,\"data\":[" + suggestions + "]}");
	check(automatic.done() && automatic.results().size() == 1 && automatic.results()[0].id == 10, "automatic scrape fetches only best game's artwork");

	Client ties("Metroid Fusion", false, false, {{true, false, false}});
	ties.receive(200, match);
	ties.receive(200, page(art(9, 5, 999, 460, 215) + "," + art(6, 5, -100, 920, 430), 2));
	check(ties.done() && ties.results()[0].artwork[0].id == 6, "heart ties are deterministic and ignore score");

	Client wrongShape("Metroid Fusion", false, false, {{true, false, false}});
	wrongShape.receive(200, match);
	wrongShape.receive(200, page(art(9, 100, 0, 600, 900), 1));
	check(wrongShape.done() && wrongShape.results()[0].artwork[0].url.empty(), "vertical artwork cannot become horizontal image");

	Client none("Metroid Fusion", false, false, {{false, false, false}});
	none.receive(200, match);
	check(none.done() && none.results().size() == 1, "disabled artwork makes no asset requests");
	Client noTitle("   ", false, false, {{true, true, true}});
	check(noTitle.done(), "empty title completes without network");

	Client fallbacks("1942 - Joint Strike (USA)", false, false, {{false, false, false}});
	unsigned int searches = 0;
	while (!fallbacks.done()) { ++searches; fallbacks.receive(200, R"({"success":true,"data":[]})"); }
	check(searches <= 3 && fallbacks.results().empty(), "no matches uses at most three queries");
	Client filtered("Metroid Fusion", false, false, {{false, false, false}});
	filtered.receive(200, R"({"success":true,"data":[{"id":1,"name":"Metroid Prime 2"}]})");
	check(filtered.done() && filtered.results().empty(), "fuzzy API suggestions do not become wrong automatic scrape");

	for (auto entry : {std::pair<int, Error>{401, Error::InvalidKey}, {403, Error::InvalidKey},
		{3, Error::Network}, {408, Error::Network}, {429, Error::RateLimited}, {503, Error::ServiceUnavailable}, {400, Error::InvalidResponse}})
	{
		Client failed("Metroid Fusion", false, false, {{true, true, true}});
		failed.receive(entry.first, "");
		check(failed.done() && failed.error() == entry.second && failed.results().empty(), "HTTP errors propagated without partial results");
	}
	for (const auto& body : {"bad json", "[]", "null", "{\"success\":false}", "{\"success\":true,\"data\":{}}",
		"{\"success\":true,\"data\":[{\"id\":\"not-a-number\",\"name\":\"Metroid Fusion\"}]}"})
	{
		Client failed("Metroid Fusion", false, false, {{true, true, true}});
		failed.receive(200, body);
		check(failed.done() && failed.error() == Error::InvalidResponse, "malformed responses rejected without crashing");
	}
	Client missingVotes("Metroid Fusion", false, false, {{true, false, false}});
	missingVotes.receive(200, match);
	missingVotes.receive(200, page(R"({"id":1,"score":900,"width":600,"height":900,"url":"https://example.com/a.png","mime":"image/png"})", 1));
	check(missingVotes.error() == Error::InvalidResponse, "missing hearts never falls back to score or upvotes");
	Client stuck("Metroid Fusion", false, false, {{true, false, false}});
	stuck.receive(200, match);
	stuck.receive(200, page("", 1));
	check(stuck.error() == Error::InvalidResponse, "empty unfinished pagination does not loop forever");
	Client endless("Metroid Fusion", false, false, {{true, false, false}});
	endless.receive(200, match);
	for (unsigned int i = 0; i < 200; ++i) endless.receive(200, page(art(1, 1, 0), 10000, i));
	check(endless.error() == Error::InvalidResponse && endless.results().empty(), "pagination bound never silently returns an incomplete winner");
	Client repeated("Metroid Fusion", false, false, {{true, false, false}});
	repeated.receive(200, match);
	repeated.receive(200, page(art(1, 1, 0), 2));
	repeated.receive(200, page(art(1, 1, 0), 2, 0));
	check(repeated.error() == Error::InvalidResponse, "wrong repeated page detected");
	Client removed("Metroid Fusion", false, false, {{true, true, true}});
	removed.receive(200, match);
	for (int i = 0; i < 3; ++i) removed.receive(200, page("", 0));
	check(removed.done() && removed.results().size() == 1, "missing artwork preserves title");
	Client failedArt("Metroid Fusion", false, false, {{true, true, true}});
	failedArt.receive(200, match);
	failedArt.receive(200, page(art(1, 1, 0), 1));
	failedArt.receive(3, "");
	check(failedArt.error() == Error::Network && failedArt.results().empty(), "mid-scrape network failure does not save partial results");
	std::cout << checks << " SteamGridDB client checks passed\n";
}

// A small line protocol lets the live Python probe drive the actual production
// client with urllib, without linking the frontend or passing secrets to it.
static int drive(const std::string& name)
{
	Client client(name, false, false, {{true, true, true}});
	while (!client.done())
	{
		rapidjson::StringBuffer requestBuffer;
        rapidjson::Writer<rapidjson::StringBuffer> request(requestBuffer);
        request.StartObject(); request.Key("path"); request.String(client.path().c_str());
        request.Key("body"); request.String(client.body().c_str()); request.EndObject();
        std::cout << "REQUEST " << requestBuffer.GetString() << std::endl;
		std::string status, body;
		if (!std::getline(std::cin, status) || !std::getline(std::cin, body)) return 2;
		client.receive(std::stoi(status), body);
	}
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> out(buffer);
	out.StartObject();
	out.Key("error"); out.Int(static_cast<int>(client.error()));
	out.Key("games"); out.StartArray();
	for (const auto& game : client.results())
	{
		out.StartObject(); out.Key("name"); out.String(game.name.c_str());
		out.Key("artwork"); out.StartArray();
		for (const auto& asset : game.artwork)
		{
			out.StartObject(); out.Key("id"); out.Uint64(asset.id); out.Key("hearts"); out.Uint64(asset.hearts);
			out.Key("url"); out.String(asset.url.c_str()); out.EndObject();
		}
		out.EndArray(); out.EndObject();
	}
	out.EndArray(); out.EndObject();
	std::cout << "RESULT " << buffer.GetString() << std::endl;
	return 0;
}

int main(int argc, char** argv)
{
	if (argc == 3 && std::string(argv[1]) == "--drive") return drive(argv[2]);
	tests();
}
