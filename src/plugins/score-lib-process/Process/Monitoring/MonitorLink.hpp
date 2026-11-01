#pragma once
#include <Process/Monitoring/Monitor.hpp>

#include <QObject>

#include <score_lib_process_export.h>

#include <functional>
#include <memory>

class QGraphicsItem;
namespace Process
{
class ProcessModel;

/**
 * Ties a view to the monitor of the process it shows.
 *
 * While the view is visible in a graphics view it counts as one of the
 * monitor's viewers, and `refresh` is called with the monitor at the display
 * rate (see MonitorLink::setRefreshInterval). `attached` is called whenever
 * the process starts or stops running, with the new monitor or null.
 *
 * All the links of the application share one timer, at the UI event rate of
 * the settings, which only runs while some link has a monitor: while a
 * monitoring process runs, with execution updates enabled. All the links of
 * one monitor are refreshed in the same tick, in which the monitor's
 * EventRing and LatestValue give each of them the same content.
 */
class SCORE_LIB_PROCESS_EXPORT MonitorLink final : public QObject
{
public:
  MonitorLink(
      const ProcessModel& process, const QGraphicsItem& view,
      std::function<void(Monitor&)> refresh,
      std::function<void(Monitor*)> attached = {}, QObject* parent = nullptr);
  ~MonitorLink() override;

  Monitor* monitor() const noexcept { return m_monitor.get(); }
  //! The monitor of a process is the one its node makes: a view knows its type
  template <typename T>
  T* monitor_as() const noexcept
  {
    return static_cast<T*>(m_monitor.get());
  }

  //! Milliseconds between two refreshes of every monitor view.
  static void setRefreshInterval(int ms);
  static int refreshInterval() noexcept;

  void tick();

private:
  void setMonitor(std::shared_ptr<Monitor> m);
  void setWatching(bool w);

  const QGraphicsItem& m_view;
  std::shared_ptr<Monitor> m_monitor;
  std::function<void(Monitor&)> m_refresh;
  std::function<void(Monitor*)> m_attached;
  bool m_watching{};
};
}
