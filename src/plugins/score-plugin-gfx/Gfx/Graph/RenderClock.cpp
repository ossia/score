#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderClock.hpp>

#include <score/tools/Timers.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

#include <algorithm>
#include <chrono>

namespace score::gfx
{

RenderClock::~RenderClock() = default;

// ---------------------------------------------------------------------------
// TimerClock
// ---------------------------------------------------------------------------

TimerClock::TimerClock(score::Timers& timers, QObject* owner, double frequencyHz)
    : m_timers{timers}
    , m_owner{owner}
    , m_frequency{frequencyHz}
{
}

TimerClock::~TimerClock()
{
  stop();
}

void TimerClock::addOutput(OutputNode* o)
{
  if(o && std::find(m_outputs.begin(), m_outputs.end(), o) == m_outputs.end())
    m_outputs.push_back(o);
}

void TimerClock::removeOutput(OutputNode* o)
{
  std::erase(m_outputs, o);
}

void TimerClock::start(std::function<void()> tick)
{
  m_tick = std::move(tick);
  if(!m_timer)
    m_timer = m_timers.acquireTimer(m_owner, m_frequency);

  // Queued connection with the owner (GfxContext) as receiver context: the
  // tick is delivered on the owner's thread — score's render thread — exactly
  // as the old connect(id, &timeout, this, &on_manual_timer, Qt::QueuedConnection).
  m_conn = QObject::connect(
      m_timer, &score::HighResolutionTimer::timeout, m_owner,
      [this](score::HighResolutionTimer*) {
        if(m_tick)
          m_tick();
      },
      Qt::QueuedConnection);
}

void TimerClock::stop()
{
  if(m_conn)
  {
    QObject::disconnect(m_conn);
    m_conn = {};
  }
  if(m_timer)
  {
    m_timers.releaseTimer(m_owner, m_timer);
    m_timer = nullptr;
  }
  m_tick = {};
}

// ---------------------------------------------------------------------------
// DisplayVSyncClock
// ---------------------------------------------------------------------------

DisplayVSyncClock::DisplayVSyncClock(OutputNode& output)
    : m_output{output}
{
}

DisplayVSyncClock::~DisplayVSyncClock()
{
  stop();
}

void DisplayVSyncClock::start(std::function<void()> tick)
{
  m_output.setVSyncCallback(std::move(tick));
}

void DisplayVSyncClock::stop()
{
  m_output.setVSyncCallback({});
}

// ---------------------------------------------------------------------------
// ExternalGenlockClock
// ---------------------------------------------------------------------------

ExternalGenlockClock::ExternalGenlockClock(
    QObject* owner, std::function<bool()> waitForNextTick)
    : m_owner{owner}
    , m_waitForNextTick{std::move(waitForNextTick)}
{
}

ExternalGenlockClock::~ExternalGenlockClock()
{
  stop();
}

void ExternalGenlockClock::start(std::function<void()> tick)
{
  if(m_running.exchange(true))
    return;
  m_tick = std::move(tick);
  m_exited.store(false, std::memory_order_release);
  m_thread = std::thread{[this] { run(); }};
}

void ExternalGenlockClock::run()
{
  while(m_running.load(std::memory_order_relaxed))
  {
    // Pull facet: block on the hardware tick (card VBI). false => timeout; loop
    // and re-check running so stop() stays responsive.
    if(m_waitForNextTick && !m_waitForNextTick())
      continue;
    if(!m_running.load(std::memory_order_relaxed))
      break;

    // Marshal the render onto the owner's (render) thread and BLOCK until it
    // returns: the next VBI wait then starts only after this frame is rendered
    // + pushed, so render can never run ahead of the card's genlock. Same
    // cross-thread hand-off PacedFramePump uses for the submit side.
    QMetaObject::invokeMethod(
        m_owner, [this] { if(m_tick) m_tick(); },
        Qt::BlockingQueuedConnection);
  }
  m_exited.store(true, std::memory_order_release);
}

void ExternalGenlockClock::stop()
{
  if(!m_running.exchange(false))
    return;

  if(m_thread.joinable())
  {
    // The tick thread may be parked in the BlockingQueuedConnection invoke,
    // waiting for m_owner's event loop to run the functor. If stop() runs on
    // the owner thread (the usual teardown case, after the event loop quits) a
    // bare join() would dead-lock. Pump the owner's event queue until the tick
    // thread has drained any in-flight blocking invoke and left its loop.
    const bool onOwnerThread
        = m_owner && QThread::currentThread() == m_owner->thread();
    while(!m_exited.load(std::memory_order_acquire))
    {
      if(onOwnerThread)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    m_thread.join();
  }
  m_tick = {};
}

}
