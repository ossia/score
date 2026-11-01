#include "MonitorLink.hpp"

#include <Process/Process.hpp>

#include <score/application/ApplicationContext.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QTimer>

#include <algorithm>
#include <vector>

namespace Process
{
namespace
{
//! The tick being refreshed, 0 outside of a refresh
uint64_t g_frame{};
}

Monitor::Monitor() noexcept = default;
Monitor::~Monitor() = default;

uint64_t monitorFrame() noexcept
{
  return g_frame;
}

namespace
{
struct MonitorClock
{
  QTimer timer;
  std::vector<MonitorLink*> links;
  //! The UI event rate the user chose: low-powered machines raise it
  int interval{std::max(1, score::AppContext().applicationSettings.uiEventRate)};
  uint64_t frame{};
  bool ticking{};

  // Never destroyed: a QTimer must not outlive the application object
  static MonitorClock& instance()
  {
    static auto* clock = new MonitorClock;
    return *clock;
  }

  MonitorClock()
  {
    timer.setTimerType(Qt::PreciseTimer);
    timer.setInterval(interval);
    QObject::connect(&timer, &QTimer::timeout, [this] { tick(); });
  }

  void tick()
  {
    // Every view refreshed in this tick reads the same content from its
    // monitor, see monitorFrame()
    g_frame = ++frame;
    ticking = true;
    // A refresh may delete a view, and with it its link: removed links are
    // nulled until the end of the tick. Links added meanwhile are refreshed
    // in this tick too.
    for(std::size_t i = 0; i < links.size(); i++)
      if(auto* l = links[i])
        l->tick();
    ticking = false;
    g_frame = 0;
    std::erase(links, nullptr);
    if(links.empty())
      timer.stop();
  }

  void add(MonitorLink* l)
  {
    if(std::find(links.begin(), links.end(), l) != links.end())
      return;
    links.push_back(l);
    if(!timer.isActive())
      timer.start();
  }

  void remove(MonitorLink* l)
  {
    if(ticking)
    {
      std::replace(links.begin(), links.end(), l, (MonitorLink*)nullptr);
      return;
    }
    std::erase(links, l);
    if(links.empty())
      timer.stop();
  }
};

bool shownInAView(const QGraphicsItem& item)
{
  if(!item.isVisible())
    return false;
  auto scene = item.scene();
  return scene && !scene->views().empty();
}
}

MonitorLink::MonitorLink(
    const ProcessModel& process, const QGraphicsItem& view,
    std::function<void(Monitor&)> refresh, std::function<void(Monitor*)> attached,
    QObject* parent)
    : QObject{parent}
    , m_view{view}
    , m_refresh{std::move(refresh)}
    , m_attached{std::move(attached)}
{
  connect(&process, &ProcessModel::monitorChanged, this, [this, &process] {
    setMonitor(process.monitor());
  });
  setMonitor(process.monitor());
}

MonitorLink::~MonitorLink()
{
  MonitorClock::instance().remove(this);
  setWatching(false);
}

void MonitorLink::setRefreshInterval(int ms)
{
  auto& c = MonitorClock::instance();
  c.interval = std::max(1, ms);
  c.timer.setInterval(c.interval);
}

int MonitorLink::refreshInterval() noexcept
{
  return MonitorClock::instance().interval;
}

void MonitorLink::setMonitor(std::shared_ptr<Monitor> m)
{
  if(m == m_monitor)
    return;
  setWatching(false);
  m_monitor = std::move(m);
  // The clock only runs while some view has something to show
  if(m_monitor)
    MonitorClock::instance().add(this);
  else
    MonitorClock::instance().remove(this);
  if(m_attached)
    m_attached(m_monitor.get());
}

void MonitorLink::setWatching(bool w)
{
  if(w == m_watching)
    return;
  m_watching = w;
  if(m_monitor)
    m_monitor->viewers.fetch_add(w ? 1 : -1, std::memory_order_relaxed);
}

void MonitorLink::tick()
{
  if(!m_monitor)
    return;
  const bool shown = shownInAView(m_view);
  setWatching(shown);
  if(shown && m_refresh)
    m_refresh(*m_monitor);
}
}
