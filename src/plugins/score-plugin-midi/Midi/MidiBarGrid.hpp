#pragma once
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>

#include <QObject>

#include <score_plugin_midi_export.h>

#include <memory>
#include <optional>
#include <vector>

#include <verdigris>

namespace Process
{
class ProcessModel;
}
namespace Scenario
{
struct Timebars;
class MusicalGrid;
class IntervalModel;
namespace Settings
{
class Model;
}
}

namespace Midi
{
//! The bar lines and their sub-divisions over a layer, in its pixels: the
//! lines musical mode draws on the timeline behind the interval holding the
//! process, computed by the same grid so that they fall where magnetism snaps.
class SCORE_PLUGIN_MIDI_EXPORT BarGrid final : public QObject
{
  W_OBJECT(BarGrid)
public:
  BarGrid(
      const Process::ProcessModel& proc, const Scenario::Settings::Model& settings,
      QObject* parent);
  ~BarGrid() override;

  struct Lines
  {
    //! Sorted x positions in the layer.
    std::vector<double> bars;
    std::vector<double> subdivisions;
  };

  void setZoom(ZoomRatio zoom);

  //! The lines between `left` and `left + width` pixels of the layer; none
  //! outside musical mode.
  const Lines& lines(double left, double width);

  //! The line, bar or sub-division, closest to `x` pixels of the layer: what
  //! a note snaps to. None outside musical mode.
  std::optional<double> nearestLine(double x);

  //! The grid moved: the lines asked for before are stale.
  void changed() E_SIGNAL(SCORE_PLUGIN_MIDI_EXPORT, changed);

private:
  //! Follows the intervals from the process's own to the one with the bars
  void watch();
  //! Where the bars fall in the layer, after one of those moved
  void place();

  const Process::ProcessModel& m_process;
  const Scenario::Settings::Model& m_settings;

  std::unique_ptr<Scenario::Timebars> m_timebars;
  std::unique_ptr<Scenario::MusicalGrid> m_grid;
  //! Around a note being snapped, apart from the lines being drawn
  std::unique_ptr<Scenario::Timebars> m_snapTimebars;
  std::unique_ptr<Scenario::MusicalGrid> m_snapGrid;

  //! The interval holding the signatures, and the date of the layer's left
  //! edge in its time base.
  const Scenario::IntervalModel* m_metrics{};
  TimeVal m_origin{};

  ZoomRatio m_zoom{};
  double m_left{};
  double m_width{-1.};
  bool m_dirty{true};

  Lines m_lines;
  std::vector<QMetaObject::Connection> m_connections;
};
}
