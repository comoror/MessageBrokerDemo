#pragma once

#include <windows.h>
#include <tchar.h>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>

#define MAX_PIPE_INSTANCES		16
#define MAX_PIPE_BUFFER_SIZE	8192

class MemBuffer {
public:
	MemBuffer() {}
	size_t GetCurrentSize() { return buffer_.size(); }
	bool AddItem(LPVOID item, size_t item_size) {
		if (item == nullptr) {
			return false;
		}
		BYTE* copy_ptr = (BYTE*)item;
		for (size_t i = 0; i < item_size; i++) {
			try {
				buffer_.emplace_back(*copy_ptr++);
			}
			catch (...) {
				return false;
			}
		}
		return true;
	}
	LPVOID AccessMem() { return buffer_.data(); }
	void ClearMemory() { buffer_.clear(); }

private:
	std::vector<BYTE> buffer_;
};

#ifdef _DEBUG
#define DBG_LOG(fmt, ...) DbgPrintf("%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define DBG_LOG(fmt, ...)
#endif

#define DBG_INFO(fmt, ...) DBG_LOG(fmt, __VA_ARGS__)
#define DBG_ERROR(fmt, ...) DBG_LOG(fmt, __VA_ARGS__)

inline void DbgPrintf(const char* format, ...)
{
	va_list argList;
	va_start(argList, format);

	char szBuffer[1024] = { 0 };
	vsprintf_s(szBuffer, format, argList);
	va_end(argList);

	OutputDebugStringA(szBuffer);
}