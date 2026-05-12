#pragma once

#include <codecvt>
#include <sstream>
#include <vector>

namespace misdk {

	namespace util_string {

		std::string wstring2utf8string(const std::wstring& str);

		std::wstring utf8string2wstring(const std::string& str);

		std::string wstring2string(const std::wstring& str);

		std::wstring string2wstring(const std::string& str);

		std::vector<std::string> explode(std::string const& s, char delim);

		std::string& trim(std::string& s);

		std::string& replace_with(std::string& str, const std::string& oldsubstr, const std::string& newsubstr);

		bool is_utf8string(std::string str);
	};
};