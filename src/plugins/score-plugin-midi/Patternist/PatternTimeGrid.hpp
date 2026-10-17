#pragma once
#include <score_plugin_midi_export.h>
#include <QObject>

#include <Patternist/PatternGrid.hpp>

#include <vector>
#include <verdigris>

Q_DECLARE_METATYPE(Patternist::TimeGrid)
W_REGISTER_ARGTYPE(Patternist::TimeGrid)

namespace Patternist
{
class ProcessModel;

//! Follows the bar grid a process plays on: the time signatures and start date
//! of the closest interval, from the process's own upwards, with a signature
//! or a tempo curve, which is where the engine reads its musical positions.
class SCORE_PLUGIN_MIDI_EXPORT TimeGridWatcher final : public QObject
{
  W_OBJECT(TimeGridWatcher)
public:
  TimeGridWatcher(const ProcessModel& proc, QObject* parent);

  const TimeGrid& grid() const noexcept { return m_grid; }

  void changed(const TimeGrid& g) W_SIGNAL(changed, g);

private:
  void update();

  const ProcessModel& m_process;
  TimeGrid m_grid;
  std::vector<QMetaObject::Connection> m_connections;
};
}
