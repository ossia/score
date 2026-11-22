#include "MidiBarGrid.hpp"

#include <Process/Process.hpp>

#include <Scenario/Document/Interval/FullView/TimeSignatureItem.hpp>
#include <Scenario/Document/Interval/FullView/Timebar.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/MusicalGrid.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <QRectF>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Midi::BarGrid)

namespace Midi
{
BarGrid::BarGrid(
    const Process::ProcessModel& proc, const Scenario::Settings::Model& settings,
    QObject* parent)
    : QObject{parent}
    , m_process{proc}
    , m_settings{settings}
    , m_timebars{std::make_unique<Scenario::Timebars>()}
{
  // Items no scene holds: the grid only fills their positions.
  for(auto* bars : {m_timebars.get(), (m_snapTimebars = std::make_unique<Scenario::Timebars>()).get()})
  {
    bars->lightBars = new Scenario::LightBars;
    bars->lighterBars = new Scenario::LighterBars;
  }
  m_grid = std::make_unique<Scenario::MusicalGrid>(*m_timebars);
  m_snapGrid = std::make_unique<Scenario::MusicalGrid>(*m_snapTimebars);

  connect(
      &m_process, &Process::ProcessModel::startOffsetChanged, this, [this] { place(); });
  connect(
      &m_settings, &Scenario::Settings::Model::MeasureBarsChanged, this,
      [this] { place(); });
  watch();
}

BarGrid::~BarGrid()
{
  for(auto& c : m_connections)
    QObject::disconnect(c);
  m_grid.reset();
  m_snapGrid.reset();
  for(auto* bars : {m_timebars.get(), m_snapTimebars.get()})
  {
    delete bars->lightBars;
    delete bars->lighterBars;
  }
}

void BarGrid::watch()
{
  for(auto& c : m_connections)
    QObject::disconnect(c);
  m_connections.clear();

  auto own = Scenario::closestParentInterval(m_process.parent());
  if(own)
  {
    // Every interval up to the one with the signatures: any can gain one,
    // which changes the chain, and their dates place the layer on its bars,
    // which only moves it.
    for(auto itv = own; itv; itv = Scenario::closestParentInterval(itv->parent()))
    {
      m_connections.push_back(connect(
          itv, &Scenario::IntervalModel::hasTimeSignatureChanged, this, [this] { watch(); }));
      m_connections.push_back(connect(
          itv, &Scenario::IntervalModel::timeSignaturesChanged, this, [this] { place(); }));
      m_connections.push_back(
          connect(itv, &Scenario::IntervalModel::dateChanged, this, [this] { place(); }));
      if(itv->hasTimeSignature())
        break;
    }
  }
  place();
}

void BarGrid::place()
{
  m_metrics = nullptr;
  if(auto own = Scenario::closestParentInterval(m_process.parent()))
  {
    const auto [model, lastFound, delta] = Scenario::closestParentWithMusicalMetrics(own);
    if(model && model->hasTimeSignature())
    {
      m_metrics = model;
      // Where Scenario::LayerData places the layer: a process that does not
      // loop itself is shifted left by its start offset.
      m_origin = (m_process.flags() & Process::ProcessFlags::HandlesLooping)
                     ? delta
                     : delta - m_process.startOffset();
    }
  }

  m_dirty = true;
  changed();
}

void BarGrid::setZoom(ZoomRatio zoom)
{
  if(zoom == m_zoom)
    return;
  m_zoom = zoom;
  m_dirty = true;
  changed();
}

const BarGrid::Lines& BarGrid::lines(double left, double width)
{
  if(!m_dirty && left == m_left && width == m_width)
    return m_lines;
  m_dirty = false;
  m_left = left;
  m_width = width;

  m_lines.bars.clear();
  m_lines.subdivisions.clear();
  if(!m_metrics || !m_settings.getMeasureBars() || m_zoom <= 0. || width <= 0.
     || m_metrics->timeSignatureMap().empty())
    return m_lines;

  m_grid->setMeasures(m_metrics->timeSignatureMap());
  m_grid->compute(
      m_origin, m_zoom, QRectF{left, 0., width, 1.},
      TimeVal::fromPixels(left, m_zoom) + m_origin);

  for(const auto& l : m_timebars->lightBars->positions)
    m_lines.bars.push_back(l.x1());
  for(const auto& l : m_timebars->lighterBars->positions)
    m_lines.subdivisions.push_back(l.x1());
  return m_lines;
}

std::optional<double> BarGrid::nearestLine(double x)
{
  if(!m_metrics || !m_settings.getMeasureBars() || m_zoom <= 0.
     || m_metrics->timeSignatureMap().empty())
    return std::nullopt;

  // Wide enough to hold a bar line at any zoom the lines are drawn at
  constexpr double window = 400.;
  const double left = x - window / 2.;
  m_snapGrid->setMeasures(m_metrics->timeSignatureMap());
  m_snapGrid->compute(
      m_origin, m_zoom, QRectF{left, 0., window, 1.},
      TimeVal::fromPixels(left, m_zoom) + m_origin);

  std::optional<double> best;
  for(const auto* lines : {&m_snapTimebars->lightBars->positions, &m_snapTimebars->lighterBars->positions})
    for(const auto& l : *lines)
      if(!best || std::abs(l.x1() - x) < std::abs(*best - x))
        best = l.x1();
  return best;
}
}
