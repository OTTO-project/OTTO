#pragma once

#include <fmt/format.h>
#include <catch.hpp>
#include <chrono>
#include <filesystem.hpp>
#include <fstream>
#include <random.hpp>

using Random = effolkronium::random_static;

namespace otto::test {
  inline fs::path dir = fs::current_path() / "testdir";

  inline void truncateFile(const filesystem::path& p)
  {
    std::fstream fstream;
    fstream.open(p.c_str(), std::ios::trunc | std::ios::out | std::ios::binary);
    fstream.close();
  }

  struct measure {
    using TimeT = std::chrono::nanoseconds;

    template<typename F, typename... Args>
    static auto execution(F&& func, Args&&... args)
    {
      auto start = std::chrono::steady_clock::now();
      std::forward<decltype(func)>(func)(std::forward<Args>(args)...);
      return std::chrono::duration_cast<TimeT>(
        std::chrono::steady_clock::now() - start);
    }
  };
} // namespace otto::test
