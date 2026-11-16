#pragma once
#include <QObject>
#include <cstdint>
#include <memory>
#include <mutex>
#include <verdigris>
#include <ossia/detail/small_flat_map.hpp>
#include <score_lib_base_export.h>

namespace score
{
//! The deadline after `deadline` for a timer of period `interval`, seen at
//! `now`: the next one of the same phase still ahead of `now`. Slots missed
//! during a stall are skipped, not caught up.
constexpr uint64_t
nextTimerDeadline(uint64_t deadline, uint64_t now, uint64_t interval) noexcept
{
  deadline += interval;
  if(deadline <= now && interval > 0)
    deadline += ((now - deadline) / interval + 1) * interval;
  return deadline;
}

class HighResolutionTimerPrivate;

class SCORE_LIB_BASE_EXPORT HighResolutionTimer : public QObject
{
  W_OBJECT(HighResolutionTimer)

public:
  explicit HighResolutionTimer(double frequencyHz, QObject* parent = nullptr);
  ~HighResolutionTimer();

  void start();
  void stop();

  bool isRunning() const;
  double frequency() const;

  void timeout(score::HighResolutionTimer* self) E_SIGNAL(SCORE_LIB_BASE_EXPORT, timeout, self);
  void started(score::HighResolutionTimer* self) E_SIGNAL(SCORE_LIB_BASE_EXPORT, started, self);
  void stopped(score::HighResolutionTimer* self) E_SIGNAL(SCORE_LIB_BASE_EXPORT, stopped, self);

  void setMaximumAccuracy(bool);
private:
  void timerEvent(QTimerEvent *k) override;
  std::unique_ptr<HighResolutionTimerPrivate> d;
};

class SCORE_LIB_BASE_EXPORT Timers : public QObject
{
  W_OBJECT(Timers)

public:
  static Timers& instance();

  explicit Timers(QObject* parent = nullptr);
  ~Timers() override;

  HighResolutionTimer* acquireTimer(QObject* user, double frequencyHz);
  void releaseTimer(QObject* user, HighResolutionTimer* timerId);

private:
  struct TimerEntry
  {
    std::unique_ptr<HighResolutionTimer> timer;
    ossia::small_flat_map<QObject*, int, 8> users;
  };

  mutable std::mutex m_mutex;
  ossia::small_flat_map<double, TimerEntry, 16> m_timers; // frequency -> entry
};
}

W_REGISTER_ARGTYPE(score::HighResolutionTimer*)
