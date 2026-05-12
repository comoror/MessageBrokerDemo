#include <Windows.h>
#include "util_string.h"

using namespace misdk;

std::string util_string::wstring2utf8string(const std::wstring& wstr)
{
	std::string strRet = "";
	CHAR* pszBuf = NULL;
	try
	{
		int cchChar = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
		if (cchChar > 0)
		{
			pszBuf = (CHAR*)malloc(cchChar * sizeof(CHAR));
			if (pszBuf)
			{
				memset(pszBuf, 0, cchChar * sizeof(CHAR));
				WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, pszBuf, cchChar, NULL, NULL);
				strRet = std::string(pszBuf);
			}
		}
	}
	catch (...)
	{

	}
	if (pszBuf)
	{
		free(pszBuf);
		pszBuf = NULL;
	}
	return strRet;
}

std::wstring util_string::utf8string2wstring(const std::string& str)
{
	std::wstring wstrRet = L"";
	WCHAR* pwszBuf = NULL;
	try
	{
		int cchWideChar = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		if (cchWideChar > 0)
		{
			pwszBuf = (WCHAR*)malloc(cchWideChar * sizeof(WCHAR));
			if (pwszBuf)
			{
				memset(pwszBuf, 0, cchWideChar * sizeof(WCHAR));
				MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, pwszBuf, cchWideChar);
				wstrRet = std::wstring(pwszBuf);
			}
		}
	}
	catch (...)
	{

	}
	if (pwszBuf)
	{
		free(pwszBuf);
		pwszBuf = NULL;
	}
	return wstrRet;
}

std::string util_string::wstring2string(const std::wstring& wstr)
{
	std::string strRet = "";
	CHAR* pszBuf = NULL;
	try
	{
		int cchChar = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
		if (cchChar > 0)
		{
			pszBuf = (CHAR*)malloc(cchChar * sizeof(CHAR));
			if (pszBuf)
			{
				memset(pszBuf, 0, cchChar * sizeof(CHAR));
				WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, pszBuf, cchChar, NULL, NULL);
				strRet = std::string(pszBuf);
			}
		}
	}
	catch (...)
	{

	}
	if (pszBuf)
	{
		free(pszBuf);
		pszBuf = NULL;
	}
	return strRet;
}

std::wstring util_string::string2wstring(const std::string& str)
{
	std::wstring wstrRet = L"";
	WCHAR* pwszBuf = NULL;
	try
	{
		int cchWideChar = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
		if (cchWideChar > 0)
		{
			pwszBuf = (WCHAR*)malloc(cchWideChar * sizeof(WCHAR));
			if (pwszBuf)
			{
				memset(pwszBuf, 0, cchWideChar * sizeof(WCHAR));
				MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, pwszBuf, cchWideChar);
				wstrRet = std::wstring(pwszBuf);
			}
		}
	}
	catch (...)
	{

	}
	if (pwszBuf)
	{
		free(pwszBuf);
		pwszBuf = NULL;
	}
	return wstrRet;
}

std::vector<std::string> util_string::explode(std::string const& s, char delim)
{
	std::vector<std::string> result;
	std::istringstream iss(s);

	for (std::string token; std::getline(iss, token, delim);)
	{
		result.push_back(token);
	}

	return result;
}

std::string& util_string::trim(std::string& s)
{
	if (s.empty())
	{
		return s;
	}

	s.erase(0, s.find_first_not_of(" "));
	s.erase(s.find_last_not_of(" ") + 1);
	return s;
}

std::string& util_string::replace_with(std::string& str, const std::string& oldsubstr, const std::string& newsubstr)
{
	size_t offset = 0;
	while ((offset = str.find(oldsubstr)) != std::string::npos)
	{
		str.replace(offset, oldsubstr.length(), newsubstr);
		offset += oldsubstr.length();
	};

	return str;
}

bool util_string::is_utf8string(std::string str)
{
	size_t nBytes = 0;
	byte chr;

	bool bAllAscii = true;
	for (int i = 0; i < str.length(); ++i)
	{
		chr = str.at(i);

		if ((chr & 0x80) != 0)	//10000000
			bAllAscii = false;

		if (nBytes == 0)
		{
			if (chr >= 0x80)
			{
				if (chr >= 0xFC && chr <= 0xFD)  //1111110x
					nBytes = 6;
				else if (chr >= 0xF8)	//111110xx
					nBytes = 5;
				else if (chr >= 0xF0)	//11110xxx 
					nBytes = 4;
				else if (chr >= 0xE0)	//1110xxxx 
					nBytes = 3;
				else if (chr >= 0xC0)	//110xxxxx 
					nBytes = 2;
				else
					return false;

				nBytes--;
			}
		}
		else
		{
			if ((chr & 0xC0) != 0x80)  //10xxxxxx
				return false;

			nBytes--;
		}
	}
	if (nBytes != 0)
		return false;

	if (bAllAscii)		//all ascii, also utf-8
		return true;

	return true;
}