#include "logger.hpp"
#include "core/globals.hpp"

#define LOGURU_IMPLEMENTATION 1
#include <loguru.hpp>

namespace otto::util::logger {
  void init(int argc, char* argv[], bool enable_console, const char* logFilePath)
  {
    if (logFilePath == nullptr) {
      logFilePath = (global::data_dir / "log.txt").c_str();
    }

    if (!enable_console) {
      loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
    }

    loguru::init(argc, argv);
    loguru::add_file(logFilePath, loguru::Append, loguru::Verbosity_MAX);

    LOG_F(INFO, "LOGGING NOW");
  }

  void set_thread_name(const std::string& name) {
    loguru::set_thread_name(name.c_str());
  }
} // namespace otto::util::logger
