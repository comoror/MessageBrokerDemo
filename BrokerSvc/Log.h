#pragma once

#define LEVEL_FATAL		0x00000005
#define LEVEL_ERROR		0x00000004
#define LEVEL_WARN		0x00000003
#define LEVEL_INFO		0x00000002
#define LEVEL_DEBUG		0x00000001
#define LEVEL_TRACE		0x00000000

#define LOG_FATAL(fmt, ...) log_event(LEVEL_FATAL, "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(fmt, ...) log_event(LEVEL_ERROR, "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_event(LEVEL_WARN,  "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_INFO(fmt, ...)	log_event(LEVEL_INFO,  "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_event(LEVEL_DEBUG, "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)
#define LOG_TRACE(fmt, ...) log_event(LEVEL_TRACE, "%hs(%d)!"##fmt, __FUNCTION__, __LINE__, __VA_ARGS__)

void log_init(const char* logger_name, const char* log_path_name, int log_level);
void log_event(int level, const char* format, ...);

