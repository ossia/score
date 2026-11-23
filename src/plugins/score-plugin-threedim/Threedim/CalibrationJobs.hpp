#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

namespace Threedim
{
//! What the objects of one calibration node share (its node_state, or the
//! object's own when it has a single instance): which of them performs the
//! side effects - the jobs it starts, the files they write - and the result of
//! the latest job, which every one of them shows. Jobs run on the worker; the
//! objects poll on the render thread.
template <typename Result>
class CalibrationJobs
{
public:
  //! Whether `who` acts for the node: the first object that asks while no
  //! other does.
  bool claim(const void* who) noexcept
  {
    if(!m_owner)
      m_owner = who;
    return m_owner == who;
  }
  void release(const void* who) noexcept
  {
    if(m_owner == who)
      m_owner = nullptr;
  }

  uint64_t start() noexcept { return ++m_started; }
  bool busy() const noexcept { return m_finished.load() < m_started.load(); }

  //! From the worker. A job finishing after a later one did is dropped.
  void finish(uint64_t id, Result r)
  {
    auto res = std::make_shared<const Result>(std::move(r));
    std::lock_guard l{m_mutex};
    if(id <= m_finished.load())
      return;
    m_result = std::move(res);
    m_finished.store(id);
  }

  //! The latest result, when it is newer than `seen`.
  std::shared_ptr<const Result> fresh(uint64_t& seen) const
  {
    if(m_finished.load() == seen)
      return {};
    std::lock_guard l{m_mutex};
    seen = m_finished.load();
    return m_result;
  }

private:
  const void* m_owner{};
  std::atomic<uint64_t> m_started{}, m_finished{};
  mutable std::mutex m_mutex;
  std::shared_ptr<const Result> m_result;
};
}
