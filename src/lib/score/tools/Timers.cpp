#include "Timers.hpp"
#include <QThread>
#include <QCoreApplication>
#include <QMutex>
#include <QDebug>
#include <cmath>
#include <ossia/detail/sleep.hpp>
#include <ossia/detail/thread_priority.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::HighResolutionTimer)
W_OBJECT_IMPL(score::Timers)

namespace score
{

class HighResolutionTimerPrivate
{
public:
  HighResolutionTimer* q{};
  QThread thread;
  std::atomic<bool> running{false};
  double frequencyHz;
  uint64_t intervalNs;

  // For jitter calculation
  double m2 = 0;  // For Welford's online variance
  double mean = 0;

  HighResolutionTimerPrivate(HighResolutionTimer* parent, double freq)
      : q(parent)
      , frequencyHz(freq)
      , intervalNs(static_cast<uint64_t>(1'000'000'000.0 / freq))
  {
    thread.setObjectName(QStringLiteral("ossia-timer"));
  }

  void timerLoop()
  {
    ossia::priority_boost_handle priorityHandle{frequencyHz};

    uint64_t nextTickNs = ossia::now_ns() + intervalNs;
    uint64_t lastTickNs = ossia::now_ns();
    uint64_t startTimeNs = lastTickNs;

    struct Stats {
      double actualFrequencyHz = 0;
      double jitterUs = 0;
      double maxLatencyUs = 0;
      uint64_t tickCount = 0;
      uint64_t missedTicks = 0;
    } stats{};

    ossia::adaptive_sleep sleeper;
    while (running.load(std::memory_order_relaxed))
    {
      sleeper.
          sleep_until(nextTickNs);

      uint64_t actualNs = ossia::now_ns();

#define HRT_STATS 1
#if defined(HRT_STATS)
      // Update statistics
      {
        stats.tickCount++;

        // Calculate actual interval and jitter using Welford's algorithm
        double actualIntervalNs = static_cast<double>(actualNs - lastTickNs);
        double delta = actualIntervalNs - mean;
        mean += delta / stats.tickCount;
        double delta2 = actualIntervalNs - mean;
        m2 += delta * delta2;

        if (stats.tickCount > 1)
        {
          const double variance = m2 / (stats.tickCount - 1);
          stats.jitterUs = std::sqrt(variance) / 1000.0;
        }

        // Latency
        int64_t latencyNs = actualNs - nextTickNs;
        if (latencyNs > 0)
        {
          double latencyUs = latencyNs / 1000.0;
          if (latencyUs > stats.maxLatencyUs)
            stats.maxLatencyUs = latencyUs;
        }

        // Actual frequency
        double elapsedS = (actualNs - startTimeNs) / 1'000'000'000.0;
        if (elapsedS > 0)
          stats.actualFrequencyHz = stats.tickCount / elapsedS;

        // Check for missed ticks
        if (latencyNs > static_cast<int64_t>(intervalNs))
        {
          stats.missedTicks += latencyNs / intervalNs;
        }

        if(stats.tickCount % 10 == 0 && stats.actualFrequencyHz > 90) {
          qDebug() << " -- actualFrequencyHz:" << stats.actualFrequencyHz
                   << " ; missed:" << stats.missedTicks
                   << " ; jitterUs:" << stats.jitterUs
                   << " ; maxLatencyUs:" << stats.maxLatencyUs
                   << " ; tickCount:" << stats.tickCount;
        }
      }
#endif

      lastTickNs = actualNs;

      // Emit signal
      q->timeout(q);

      // Schedule next tick
      nextTickNs += intervalNs;

      // If we've fallen too far behind, reset
      if (actualNs > nextTickNs + intervalNs * 3)
      {
        nextTickNs = actualNs + intervalNs;
      }
    }

  }
};

HighResolutionTimer::HighResolutionTimer(double frequencyHz, QObject* parent)
    : QObject(parent)
    , d(std::make_unique<HighResolutionTimerPrivate>(this, frequencyHz))
{
}

HighResolutionTimer::~HighResolutionTimer()
{
  stop();
}

void HighResolutionTimer::start()
{
  if (d->running.exchange(true))
    return;

  QObject::connect(&d->thread, &QThread::started, [this]() {
    d->timerLoop();
  });

  d->thread.start(QThread::TimeCriticalPriority);
  started(this);
}

void HighResolutionTimer::stop()
{
  if (!d->running.exchange(false))
    return;

  d->thread.quit();
  d->thread.wait();

  QObject::disconnect(&d->thread, &QThread::started, nullptr, nullptr);
  stopped(this);
}

bool HighResolutionTimer::isRunning() const
{
  return d->running.load();
}

double HighResolutionTimer::frequency() const
{
  return d->frequencyHz;
}

Timers& Timers::instance()
{
  static Timers s_instance;
  return s_instance;
}

Timers::Timers(QObject* parent)
    : QObject(parent)
{
}

Timers::~Timers()
{
  std::lock_guard lock{m_mutex};
  for (auto& [k, entry] : m_timers)
  {
    entry.timer->stop();
  }
  m_timers.clear();
}

HighResolutionTimer* Timers::acquireTimer(QObject* user, double frequencyHz)
{
  std::lock_guard lock{m_mutex};
  if (auto it = m_timers.find(frequencyHz); it != m_timers.end())
  {
    it->second.users[user]++;
    return it->second.timer.get();
  }

  TimerEntry entry;
  entry.timer = std::make_unique<HighResolutionTimer>(frequencyHz, this);
  entry.users[user] = 1;
  entry.timer->start();

  auto* ptr = entry.timer.get();
  m_timers.emplace(frequencyHz, std::move(entry));

  return ptr;
}

void Timers::releaseTimer(QObject* user, HighResolutionTimer* timer)
{
  if(!timer)
    return;

  std::lock_guard lock{m_mutex};
  for (auto it = m_timers.begin(); it != m_timers.end(); ++it)
  {
    if (it->second.timer.get() == timer)
    {
      if (--it->second.users[user] == 0)
      {
        disconnect(it->second.timer.get(), nullptr, user, nullptr);
        it->second.users.erase(user);
        if(it->second.users.empty()) {
          it->second.timer->stop();
          m_timers.erase(it);
        }
      }
      return;
    }
  }
}

}
