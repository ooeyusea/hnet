#ifndef __HNET_LOGGER_H__
#define __HNET_LOGGER_H__
#include "spdlog/spdlog.h"
extern std::shared_ptr<spdlog::logger> g_logger;

#define hn_trace(format, ...) g_logger->trace("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#define hn_debug(format, ...) g_logger->debug("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#define hn_info(format, ...) g_logger->info("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#define hn_warn(format, ...) g_logger->warn("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#define hn_error(format, ...) g_logger->error("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#define hn_critical(format, ...) g_logger->critical("[{}:{}]"#format, __LINE__, __FILE__, ##__VA_ARGS__)
#endif // !__HNET_LOGGER_H__
