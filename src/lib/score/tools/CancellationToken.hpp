#pragma once
#include <score_lib_base_export.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace score
{
class SCORE_LIB_BASE_EXPORT CancellationToken
{
public:
  static std::shared_ptr<CancellationToken> create();
  static void cancelAll() noexcept;

  bool cancelled() const noexcept { return m_cancelled; }
  void cancel() noexcept;

  template <typename F>
  bool run(F&& f)
  {
    std::lock_guard lock{m_mutex};
    if(m_cancelled)
      return false;
    f();
    return true;
  }

private:
  std::atomic_bool m_cancelled{false};
  std::mutex m_mutex;
};
}
