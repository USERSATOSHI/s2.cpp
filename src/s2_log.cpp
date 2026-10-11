#include "../include/s2_log.h"
#include "ggml.h"

#include <algorithm>
#include <cstdio>
#include <mutex>

namespace s2 {
namespace {

std::atomic<int32_t> g_log_level{static_cast<int32_t>(LogLevel::Info)};
ggml_log_callback g_backend_log_callback = nullptr;
void * g_backend_log_data = nullptr;
std::once_flag g_backend_logging_once;

void backend_log(enum ggml_log_level level, const char * text, void *) {
    // Graph replay emits a debug line at every codebook. Keep the existing
    // backend callback, while respecting S2's debug setting for those lines.
    if (level == GGML_LOG_LEVEL_DEBUG && !log_enabled(LogLevel::Debug)) return;
    if (g_backend_log_callback) g_backend_log_callback(level, text, g_backend_log_data);
    else std::fputs(text, stderr);
}

}

void set_log_level(LogLevel level) {
    const int32_t clamped = std::max(
        static_cast<int32_t>(LogLevel::Error),
        std::min(static_cast<int32_t>(LogLevel::Debug), static_cast<int32_t>(level)));
    g_log_level.store(clamped, std::memory_order_relaxed);
}

LogLevel get_log_level() {
    return static_cast<LogLevel>(g_log_level.load(std::memory_order_relaxed));
}

bool log_enabled(LogLevel level) {
    return static_cast<int32_t>(level) <= g_log_level.load(std::memory_order_relaxed);
}

void init_backend_logging() {
    std::call_once(g_backend_logging_once, [] {
        ggml_log_get(&g_backend_log_callback, &g_backend_log_data);
        ggml_log_set(backend_log, nullptr);
    });
}

}
