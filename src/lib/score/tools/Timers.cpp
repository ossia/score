#include "Timers.hpp"

#include <QThread>
#include <QCoreApplication>
#include <QMutex>
#include <cmath>

// Platform detection
#if defined(_WIN32)
#define HRT_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define HRT_PLATFORM_MACOS
#elif defined(__linux__)
#define HRT_PLATFORM_LINUX
#else
#error "Unsupported platform"
#endif

// Platform includes
#ifdef HRT_PLATFORM_WINDOWS
#include <Windows.h>
#include <avrt.h>
#pragma comment(lib, "avrt.lib")
#pragma comment(lib, "winmm.lib")
#endif

#ifdef HRT_PLATFORM_MACOS
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <pthread.h>
#endif

#ifdef HRT_PLATFORM_LINUX
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::HighResolutionTimer)
W_OBJECT_IMPL(score::Timers)

namespace score
{
namespace {

// Returns current time in nanoseconds
inline uint64_t nowNs()
{
#ifdef HRT_PLATFORM_WINDOWS
  static LARGE_INTEGER freq = []() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
  }();
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  return static_cast<uint64_t>(now.QuadPart * 1'000'000'000ULL / freq.QuadPart);
#endif

#ifdef HRT_PLATFORM_MACOS
  static mach_timebase_info_data_t timebase = []() {
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    return tb;
  }();
  return mach_absolute_time() * timebase.numer / timebase.denom;
#endif

#ifdef HRT_PLATFORM_LINUX
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_nsec;
#endif
}

// Sleep until absolute time (nanoseconds)
inline void sleepUntilNs(uint64_t targetNs)
{
#ifdef HRT_PLATFORM_WINDOWS
  // Convert to QPC ticks
  static LARGE_INTEGER freq = []() {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
  }();

  uint64_t currentNs = nowNs();
  if (targetNs <= currentNs)
    return;

  int64_t remainingNs = targetNs - currentNs;
  int64_t remainingMs = remainingNs / 1'000'000;

  // Coarse sleep for bulk of time
  if (remainingMs > 2)
  {
    Sleep(static_cast<DWORD>(remainingMs - 1));
  }

  // Spin for final precision
  while (nowNs() < targetNs)
  {
    YieldProcessor();
  }
#endif

#ifdef HRT_PLATFORM_MACOS
  static mach_timebase_info_data_t timebase = []() {
    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);
    return tb;
  }();

  // Convert ns to mach absolute time
  uint64_t machTarget = targetNs * timebase.denom / timebase.numer;
  mach_wait_until(machTarget);
#endif

#ifdef HRT_PLATFORM_LINUX
  struct timespec ts;
  ts.tv_sec = targetNs / 1'000'000'000ULL;
  ts.tv_nsec = targetNs % 1'000'000'000ULL;

  // TIMER_ABSTIME means we specify absolute time, not relative
  // This automatically handles interruptions and provides drift-free timing
  while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, nullptr) != 0)
  {
    // EINTR - interrupted by signal, retry
  }
#endif
}

// Platform-specific thread priority handle
struct PriorityBoostHandle
{
#ifdef HRT_PLATFORM_WINDOWS
  HANDLE mmcssHandle = nullptr;
  UINT timerResolution = 1;
#endif
#ifdef HRT_PLATFORM_MACOS
  bool policySet = false;
#endif
#ifdef HRT_PLATFORM_LINUX
  int originalPolicy = SCHED_OTHER;
  struct sched_param originalParam = {};
  bool elevated = false;
#endif
};

inline PriorityBoostHandle acquirePriorityBoost(double frequencyHz)
{
  PriorityBoostHandle handle;

#ifdef HRT_PLATFORM_WINDOWS
  // Timer resolution already increased in main()

  // MMCSS priority boost
  DWORD taskIndex = 0;
  handle.mmcssHandle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
  if (handle.mmcssHandle)
  {
    AvSetMmThreadPriority(handle.mmcssHandle, AVRT_PRIORITY_CRITICAL);
  }
#endif

#ifdef HRT_PLATFORM_MACOS
  // Set real-time thread policy with time constraint
  // This tells the scheduler we need periodic execution
  mach_port_t threadPort = pthread_mach_thread_np(pthread_self());

  // Get base clock rate for conversion
  mach_timebase_info_data_t timebase;
  mach_timebase_info(&timebase);

  // Convert frequency to period in mach absolute time units
  double periodNs = 1'000'000'000.0 / frequencyHz;
  uint32_t periodMach = static_cast<uint32_t>(periodNs * timebase.denom / timebase.numer);

  thread_time_constraint_policy_data_t policy;
  policy.period = periodMach;                          // Nominal period
  policy.computation = periodMach / 10;                // Max computation time (~10% of period)
  policy.constraint = periodMach / 2;                  // Must complete within half period
  policy.preemptible = 1;                              // Can be preempted

  kern_return_t kr = thread_policy_set(
      threadPort,
      THREAD_TIME_CONSTRAINT_POLICY,
      reinterpret_cast<thread_policy_t>(&policy),
      THREAD_TIME_CONSTRAINT_POLICY_COUNT
      );

  handle.policySet = (kr == KERN_SUCCESS);

  if (!handle.policySet)
  {
    // Fallback: at least set high QoS
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
  }
#endif

#ifdef HRT_PLATFORM_LINUX
  // Save original scheduling policy
  pthread_getschedparam(pthread_self(), &handle.originalPolicy, &handle.originalParam);

  // Try to set SCHED_FIFO (requires CAP_SYS_NICE or root)
  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);

  if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0)
  {
    handle.elevated = true;
  }
  else
  {
    // Fallback: set highest nice value we can
    setpriority(PRIO_PROCESS, 0, -20);
  }
#endif

  return handle;
}

inline void releasePriorityBoost(PriorityBoostHandle& handle)
{
#ifdef HRT_PLATFORM_WINDOWS
  if (handle.mmcssHandle)
  {
    AvRevertMmThreadCharacteristics(handle.mmcssHandle);
    handle.mmcssHandle = nullptr;
  }
#endif

#ifdef HRT_PLATFORM_MACOS
  if (handle.policySet)
  {
    // Revert to default policy
    mach_port_t threadPort = pthread_mach_thread_np(pthread_self());
    thread_standard_policy_data_t policy;
    thread_policy_set(
        threadPort,
        THREAD_STANDARD_POLICY,
        reinterpret_cast<thread_policy_t>(&policy),
        THREAD_STANDARD_POLICY_COUNT
        );
    handle.policySet = false;
  }
#endif

#ifdef HRT_PLATFORM_LINUX
  if (handle.elevated)
  {
    pthread_setschedparam(pthread_self(), handle.originalPolicy, &handle.originalParam);
    handle.elevated = false;
  }
#endif
}

}

class HighResolutionTimerPrivate
{
public:
  HighResolutionTimer* q{};
  QThread thread;
  std::atomic<bool> running{false};
  double frequencyHz;
  uint64_t intervalNs;

  // Statistics
  mutable QMutex statsMutex;
  HighResolutionTimer::Stats stats;

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
    PriorityBoostHandle priorityHandle = acquirePriorityBoost(frequencyHz);

    uint64_t nextTickNs = nowNs() + intervalNs;
    uint64_t lastTickNs = nowNs();
    uint64_t startTimeNs = lastTickNs;

    while (running.load(std::memory_order_relaxed))
    {
      sleepUntilNs(nextTickNs);

      uint64_t actualNs = nowNs();

      // Update statistics
      {
        QMutexLocker lock(&statsMutex);
        stats.tickCount++;

        // Calculate actual interval and jitter using Welford's algorithm
        double actualIntervalNs = static_cast<double>(actualNs - lastTickNs);
        double delta = actualIntervalNs - mean;
        mean += delta / stats.tickCount;
        double delta2 = actualIntervalNs - mean;
        m2 += delta * delta2;

        if (stats.tickCount > 1)
        {
          double variance = m2 / (stats.tickCount - 1);
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
      }

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

    releasePriorityBoost(priorityHandle);
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

  // Reset stats
  {
    QMutexLocker lock(&d->statsMutex);
    d->stats = Stats{};
    d->m2 = 0;
    d->mean = 0;
  }

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

HighResolutionTimer::Stats HighResolutionTimer::stats() const
{
  QMutexLocker lock(&d->statsMutex);
  return d->stats;
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
