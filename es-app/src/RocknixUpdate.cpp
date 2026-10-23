#include "RocknixUpdate.h"

#include <curl/curl.h>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace RocknixUpdate
{
	static curl_off_t downloadSize(const std::string& url)
	{
		CURL* curl = curl_easy_init();
		if (!curl)
			return 0;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
		curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, "EmulationStation ROCKNIX updater");
		curl_off_t size = 0;
		if (curl_easy_perform(curl) == CURLE_OK)
			curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &size);
		curl_easy_cleanup(curl);
		return size > 0 ? size : 0;
	}

	// remove_all unlinks child symlinks instead of following them. Keep the folder itself.
	static bool clearContents(const std::string& directory)
	{
		std::error_code error;
		auto status = std::filesystem::symlink_status(directory, error);
		if (status.type() == std::filesystem::file_type::not_found)
			return true;
		if (error || !std::filesystem::is_directory(status))
			return false;
		std::filesystem::directory_iterator entry(directory, error), end;
		while (!error && entry != end)
		{
			std::filesystem::remove_all(entry->path(), error);
			if (!error)
				entry.increment(error);
		}
		return !error;
	}

	std::pair<std::string, int> run(const std::string& command, const std::string& directory,
		const std::string& logPath, const std::function<void(const std::string)>& notify)
	{
		FILE* pipe = popen(command.c_str(), "r");
		FILE* log = fopen(logPath.c_str(), "w");
		std::string lastLine = "Cannot call update command";
		int exitCode = -1;
		if (pipe)
		{
			std::string pending, file;
			curl_off_t total = 0;
			bool downloading = false;
			auto nextProgress = std::chrono::steady_clock::now();
			auto onLine = [&](const std::string& line) {
				lastLine = line;
				if (log)
					fprintf(log, "%s\n", line.c_str());
				if (notify)
					notify(line);

				const std::string prefix = "Update available at: ";
				if (line.compare(0, prefix.size(), prefix) == 0)
				{
					const auto url = line.substr(prefix.size());
					const auto name = url.substr(url.find_last_of('/') + 1);
					if (!name.empty() && name != "." && name != "..")
					{
						file = directory + "/" + name;
						// The updater starts curl immediately after printing this URL, so
						// this bounded HEAD request can overlap the beginning of the download.
						total = downloadSize(url);
					}
				}
				// Checksum download and verification retain their own status text.
				downloading = line == "Downloading update file...";
			};

			// Poll stdout so a quiet download still gets progress updates every second.
			pollfd output{fileno(pipe), POLLIN, 0};
			for (;;)
			{
				const int ready = poll(&output, 1, 200);
				if (ready < 0)
				{
					if (errno == EINTR)
						continue;
					break;
				}
				if (ready > 0)
				{
					char buffer[1024];
					const auto count = read(output.fd, buffer, sizeof(buffer));
					if (count < 0 && errno == EINTR)
						continue;
					if (count <= 0)
						break;
					pending.append(buffer, count);
					size_t newline;
					while ((newline = pending.find('\n')) != std::string::npos)
					{
						onLine(pending.substr(0, newline));
						pending.erase(0, newline + 1);
					}
				}
				const auto now = std::chrono::steady_clock::now();
				if (notify && downloading && total > 0 && now >= nextProgress)
				{
					std::error_code error;
					const auto status = std::filesystem::symlink_status(file, error);
					if (!error && std::filesystem::is_regular_file(status))
					{
						const auto size = std::filesystem::file_size(file, error);
						if (!error)
						{
							const int percent = size >= static_cast<uintmax_t>(total) ? 100 :
								static_cast<int>(100.0 * size / total);
							notify("Downloading update file... >>> " + std::to_string(percent) + "%");
						}
					}
					nextProgress = now + std::chrono::seconds(1);
				}
			}
			if (!pending.empty())
				onLine(pending);
			const int status = pclose(pipe);
			if (status != -1 && WIFEXITED(status))
				exitCode = WEXITSTATUS(status);
		}

		// Wait until the updater has exited before removing partial or invalid updates.
		if (exitCode != 0 && !clearContents(directory))
			lastLine += " (failed to clear update folder)";
		if (log)
		{
			fprintf(log, "Exit code : %d\n%s\n", exitCode, lastLine.c_str());
			fclose(log);
		}
		return {lastLine, exitCode};
	}
}
