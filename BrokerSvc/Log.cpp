#include <windows.h>
#include <stdio.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "Log.h"

static std::shared_ptr<spdlog::logger> spd_logger;

void log_init(const char* logger_name, const char* log_path_name, int log_level)
{
	// Create a file rotating logger with 10 MB size max and 3 rotated files
	constexpr int max_size = 10 * 1024 * 1024;
	constexpr int max_files = 3;

	spd_logger = spdlog::rotating_logger_mt(logger_name, log_path_name, max_size, max_files);

	spd_logger->set_level((spdlog::level::level_enum)log_level);
	spd_logger->flush_on((spdlog::level::level_enum)log_level);
	spd_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][%P][%t][%l]%v");
}

void log_event(int level, const char* format, ...)
{
	va_list argList;
	va_start(argList, format);

	char szBuffer[1024] = { 0 };
	vsprintf_s(szBuffer, format, argList);
	va_end(argList);

	switch (level)
	{
	case LEVEL_FATAL:
		spd_logger->critical(szBuffer);
		break;
	case LEVEL_ERROR:
		spd_logger->error(szBuffer);
		break;
	case LEVEL_WARN:
		spd_logger->warn(szBuffer);
		break;
	case LEVEL_INFO:
		spd_logger->info(szBuffer);
		break;
	case LEVEL_DEBUG:
		spd_logger->debug(szBuffer);
		break;
	case LEVEL_TRACE:
		spd_logger->trace(szBuffer);
		break;
	default:
		break;
	}
}