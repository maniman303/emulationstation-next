# SteamGridDB scraper

Select **SteamGridDB** in the scraper menu and enter the key under
**Accounts → STEAMGRIDDB → API KEY**. The existing `SteamGridDBApiKey` string
setting in `es_settings.cfg` stores the key. The GUI masks it.

The scraper imports the title and these artwork fields:

| Artwork | Gamelist field |
| --- | --- |
| Steam horizontal grid, 460×215 or 920×430 | `image` |
| Steam square grid, 512×512 or 1024×1024 | `thumbnail` |
| Logo | `marquee` |

Each source can be disabled with **NONE** in scraper settings. Existing
overwrite settings apply. Other metadata, including descriptions, manuals,
release dates and ScreenScraper IDs, is preserved. There is no system filter.

Filename searches remove release tags, normalize whitespace/separators and
recognize catalogue prefixes. They try at most three distinct queries and
stop early for a strong title match. Explicit manual search text is preserved.
Matching accounts for Latin accents, trailing articles and generic “Version”
suffixes, and rejects mismatched sequel numbers. Automatic scraping fetches
artwork for the best match; manual scraping returns at most five matches.

Artwork is selected by **hearts**, independently of score and upvotes, across
all result pages. Ties prefer resolution, then the lowest asset ID. Only static
PNG/JPEG grids and PNG logos are requested. Successfully fetched API pages are
cached in memory for ten minutes, with a maximum of 128 entries; changing the
API key clears the cache. Disabled sources and existing artwork in a bulk
scrape without overwrite avoid artwork lookups.

## API compatibility

Authenticated game searches use the documented
[`/api/v2/search/autocomplete` API](https://www.steamgriddb.com/api/v2).
API v2 artwork responses provide score/upvotes but omit the website's separate
heart counts. Artwork therefore uses the website's public, read-only
`POST /api/public/search/assets` search endpoint, whose results include `hearts`.
This endpoint is not part of the documented v2 contract and may change. The
client reports an error for an incompatible response rather than substituting
scores or upvotes or saving an incomplete result.

Missing/invalid keys, connection failures, rate limits and service errors are
reported distinctly. API requests time out after 20 seconds; image downloads
time out after 60 seconds. Bulk scraping uses the same error-code handling as
the other scrapers: fatal errors such as invalid credentials stop the scrape;
other errors are recorded and the queue continues. Normal empty search/artwork
results are successful and preserve existing metadata.

## Local checks

From the repository root, compile and run the standalone production-client
checks (requires a host C++17 compiler and RapidJSON headers):

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ies-app/src tests/steamgriddb-client.cpp es-app/src/scrapers/SteamGridDBClient.cpp -o /tmp/steamgriddb-tests
/tmp/steamgriddb-tests
```

Run the optional live API checks using a hidden key prompt:

```sh
python3 tests/steamgriddb-live.py /tmp/steamgriddb-tests
```

The probe drives the same client used by the frontend. It checks missing/wrong
authentication, numbered filenames, empty results, pagination, and the chosen
artwork against the heart counts on every retrieved page. It does not store
the key or write to the account. Fixture checks also cover network failures,
rate limits, malformed responses, bounded pagination, and disagreements between
hearts, upvotes and score.
