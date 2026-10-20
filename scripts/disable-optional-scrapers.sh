#!/bin/sh

# Source this file to remove these settings from the current shell and the
# persistent ROCKNIX build options file.
(return 0 2>/dev/null) && sourced=1 || sourced=0

main()
{
	options_file="${HOME}/.ROCKNIX/options"

	if [ -f "$options_file" ]; then
		tmp_file=$(mktemp "${options_file}.tmp.XXXXXX") || {
			printf 'Unable to create a temporary file beside %s\n' "$options_file" >&2
			return 1
		}

		if ! awk '
			/^[[:space:]]*(export[[:space:]]+)?(SCREENSCRAPER_DEV_LOGIN|GAMESDB_APIKEY)[[:space:]]*=/ { next }
			{ print }
		' "$options_file" > "$tmp_file"; then
			rm -f "$tmp_file"
			printf 'Unable to update %s\n' "$options_file" >&2
			return 1
		fi

		chmod --reference="$options_file" "$tmp_file" || {
			rm -f "$tmp_file"
			printf 'Unable to preserve permissions on %s\n' "$options_file" >&2
			return 1
		}

		mv "$tmp_file" "$options_file" || {
			rm -f "$tmp_file"
			printf 'Unable to replace %s\n' "$options_file" >&2
			return 1
		}
	fi

	unset SCREENSCRAPER_DEV_LOGIN GAMESDB_APIKEY
	if [ "$sourced" -eq 1 ]; then
		printf 'Removed SCREENSCRAPER_DEV_LOGIN and GAMESDB_APIKEY from the current shell and %s\n' "$options_file"
	else
		printf 'Removed SCREENSCRAPER_DEV_LOGIN and GAMESDB_APIKEY from %s\n' "$options_file"
		printf 'Source this script to clear them from the current shell too.\n'
	fi
}

main
result=$?

if [ "$sourced" -eq 1 ]; then
	return "$result"
fi

exit "$result"
