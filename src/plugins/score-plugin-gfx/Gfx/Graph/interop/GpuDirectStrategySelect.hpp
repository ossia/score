#pragma once

/**
 * @file GpuDirectStrategySelect.hpp
 * @brief Generic "try GPU-direct output strategies in order" picker.
 *
 * Every output addon walks the same DVP -> Tier3-RDMA -> (CPU) candidate chain:
 * construct a strategy, init() it, on success keep it, on failure release and
 * try the next. Only the candidate *list* (which is backend- and vendor-
 * specific, gated by #if macros) differs. This helper owns the init/log/release/
 * fallback loop so each addon just builds the candidate factories.
 *
 * The per-vendor strategy classes stay in the addon (they hold the card handle
 * and pixel-format enum); the addon passes factory thunks that construct them.
 */

#include <Gfx/Graph/interop/GpuDirectStrategy.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace score::gfx::interop
{

/// Try each candidate factory in order against `cfg`. The first whose init()
/// succeeds is returned (ownership transfers to the caller, which must release()
/// it at shutdown). Failed candidates are release()d and skipped, as are null
/// factories and null results. Returns nullptr if none succeed.
///
/// `onEngaged(name)` / `onFailed(name)` are optional logging callbacks invoked
/// with the strategy's name() so the caller can report which path engaged.
inline std::unique_ptr<GpuDirectStrategy> selectGpuDirectStrategy(
    const GpuDirectStrategyConfig& cfg,
    std::vector<std::function<std::unique_ptr<GpuDirectStrategy>()>> candidates,
    const std::function<void(const char*)>& onEngaged = {},
    const std::function<void(const char*)>& onFailed = {})
{
  for(auto& make : candidates)
  {
    if(!make)
      continue;
    auto s = make();
    if(!s)
      continue;
    if(s->init(cfg))
    {
      if(onEngaged)
        onEngaged(s->name());
      return s;
    }
    if(onFailed)
      onFailed(s->name());
    s->release();
  }
  return nullptr;
}

} // namespace score::gfx::interop
