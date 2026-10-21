#pragma once
#ifndef ES_CORE_HELP_PROMPT_H
#define ES_CORE_HELP_PROMPT_H

#include <string>
#include <functional>

struct HelpPrompt
{
	HelpPrompt() { }

	HelpPrompt(const std::string& button, const std::string& label, const std::function<void()>& onclick = nullptr)
	{
		first = button;
		second = label;
		action = onclick;
	}

	// Non-negative values override the default help order for a specific screen.
	int sortOrder = -1;

	std::string first;
	std::string second;	
	
	std::function<void()> action;
};

//typedef std::pair<std::string, std::string> HelpPrompt;

#endif // ES_CORE_HELP_PROMPT_H
