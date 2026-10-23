#pragma once

#include <functional>
#include <string>
#include <utility>

namespace RocknixUpdate
{
	std::pair<std::string, int> run(const std::string& command, const std::string& directory,
		const std::string& logPath, const std::function<void(const std::string)>& notify);
}
