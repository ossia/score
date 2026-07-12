#pragma once

/**
 * @file StageProfiler.hpp
 * @brief Env-gated per-stage wall-time accumulator for the GPU-direct
 *        video pipelines.
 *
 * Zero overhead unless SCORE_AJA_PROFILE=1 is set in the environment
 * (one branch on a cached bool). Each profiled stage prints its running
 * mean every `printEvery` samples to stderr:
 *
 *     [prof] texgen-paint                 n=240 mean=41.273 ms
 *
 * Intended for the roundtrip harnesses and one-off perf investigations —
 * not a tracing system. For GPU-side truth use nsys; this attributes
 * *render-thread wall time* to pipeline stages, which is what gates fps
 * when the producer is the bottleneck.
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>

namespace score::gfx::interop
{

struct StageProfiler
{
  static bool enabled() noexcept
  {
    static const bool e = [] {
      const char* v = std::getenv("SCORE_AJA_PROFILE");
      return v && *v && *v != '0';
    }();
    return e;
  }

  const char* name{};
  int printEvery{240};
  std::atomic<long long> totalNs{0};
  std::atomic<long long> count{0};

  explicit StageProfiler(const char* n, int every = 32) noexcept
      : name{n}
      , printEvery{every}
  {
  }

  ~StageProfiler()
  {
    // Final dump at process exit so short/slow runs still report.
    const auto c = count.load(std::memory_order_relaxed);
    if(enabled() && c > 0)
    {
      std::fprintf(
          stderr, "[prof-final] %-28s n=%6lld  mean=%9.3f ms\n", name,
          (long long)c,
          double(totalNs.load(std::memory_order_relaxed)) / double(c) / 1e6);
    }
  }

  void add(long long ns) noexcept
  {
    totalNs.fetch_add(ns, std::memory_order_relaxed);
    const auto c = count.fetch_add(1, std::memory_order_relaxed) + 1;
    if(c % printEvery == 0)
    {
      std::fprintf(
          stderr, "[prof] %-28s n=%6lld  mean=%9.3f ms\n", name, (long long)c,
          double(totalNs.load(std::memory_order_relaxed)) / double(c) / 1e6);
    }
  }

  struct Scope
  {
    StageProfiler* p{};
    std::chrono::steady_clock::time_point t0;

    explicit Scope(StageProfiler& prof) noexcept
        : p{StageProfiler::enabled() ? &prof : nullptr}
    {
      if(p)
        t0 = std::chrono::steady_clock::now();
    }

    ~Scope()
    {
      if(p)
        p->add(std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now() - t0)
                   .count());
    }
  };
};

/// Profile the enclosing scope under `label`. One static accumulator per
/// call site; prints running means every 240 samples when enabled.
#define SCORE_STAGE_PROFILE(var, label)                       \
  static ::score::gfx::interop::StageProfiler var{label};     \
  ::score::gfx::interop::StageProfiler::Scope var##_scope { var }

} // namespace score::gfx::interop
