#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderClock.hpp>

#include <score/tools/Timers.hpp>

#include <algorithm>

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

}
