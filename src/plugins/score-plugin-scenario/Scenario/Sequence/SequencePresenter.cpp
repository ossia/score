// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequencePresenter.hpp"

#include <Automation/AutomationModel.hpp>

#include <Color/GradientModel.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/PortItem.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalView.hpp>
#include <Scenario/Sequence/Commands/MoveSequenceIS.hpp>

#include <Process/TimeValue.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequencePresenter)

namespace Sequence
{

SequencePresenter::SequencePresenter(
    const SequenceModel& model, SequenceView* view, const Process::Context& ctx,
    QObject* parent)
    : Process::LayerPresenter{model, view, ctx, parent}
    , m_model{model}
    , m_view{*view}
{
  // Update handles + sections when model structure changes
  connect(
      &model, &SequenceModel::structureChanged, this,
      [this] {
        rebuildSections();
        updateHandles();
      });

  // Handle drag: dispatch the ongoing move command — plain (neighbours
  // resize) or ripple (shift held: what follows shifts, parent end moves).
  connect(
      view, &SequenceView::handleDragMoved, this,
      [this](Id<Scenario::TimeSyncModel> tsId, double newX, bool ripple) {
        if(m_zoom <= 0)
          return;
        const auto newDate = TimeVal::fromPixels(newX, m_zoom);
        if(ripple)
          m_context.context.dispatcher
              .submit<Sequence::Command::MoveSequenceISRipple>(
                  m_model, tsId, newDate);
        else
          m_context.context.dispatcher.submit<Sequence::Command::MoveSequenceIS>(
              m_model, tsId, newDate);
      });

  connect(
      view, &SequenceView::handleDragReleased, this,
      [this](Id<Scenario::TimeSyncModel>, double, bool) {
        m_context.context.dispatcher.commit();
      });

  connect(view, &SequenceView::handleDragCancelled, this, [this]() {
    m_context.context.dispatcher.rollback();
    updateHandles();
  });

  // Rail double-click: insert an IS at that date, splitting the section and
  // its automation curves.
  connect(view, &SequenceView::railDoubleClicked, this, [this](double x) {
    if(m_zoom <= 0)
      return;
    const auto date = TimeVal::fromPixels(x, m_zoom);
    auto cmd = new Sequence::Command::InsertSequenceIS{m_model, date};
    if(cmd->valid())
      CommandDispatcher<>{m_context.context.commandStack}.submit(cmd);
    else
      delete cmd;
  });

  // Build section presenters for initial model state.
  // Zoom hasn't been set yet so we defer layout to on_zoomRatioChanged.
  rebuildSections();
}

SequencePresenter::~SequencePresenter()
{
  qDeleteAll(m_rowPorts);
  qDeleteAll(m_sectionPresenters);
}

void SequencePresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view.setWidth(width);
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::setHeight(qreal height)
{
  m_view.setHeight(height);
  // Child section presenters manage their own height based on slot content;
  // we only propagate the full height so each one knows the available space.
  for(auto* p : m_sectionPresenters)
    p->view()->setHeight(height);
}

void SequencePresenter::putToFront()
{
  m_view.setVisible(true);
}

void SequencePresenter::putBehind()
{
  m_view.setVisible(false);
}

void SequencePresenter::on_zoomRatioChanged(ZoomRatio ratio)
{
  m_zoom = ratio;
  for(auto* p : m_sectionPresenters)
    p->on_zoomRatioChanged(ratio);
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::parentGeometryChanged()
{
  updateHandles();
  updateSectionLayout();
}

void SequencePresenter::rebuildSections()
{
  qDeleteAll(m_sectionPresenters);
  m_sectionPresenters.clear();

  // Track the reference section's rack layout so the row ports follow
  // slot resizes / front-process switches / reorganizations.
  for(auto& conn : m_rackConns)
    disconnect(conn);
  m_rackConns.clear();
  if(auto ord = m_model.orderedIntervals(); !ord.empty())
  {
    auto& first = m_model.intervals.at(ord.front());
    const auto upd = [this] { updateRowPorts(); };
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::rackChanged, this, [upd](auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::slotResized, this, [upd](auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::slotAdded, this, [upd](auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::slotRemoved, this, [upd](auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::slotsSwapped, this,
        [upd](auto, auto, auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::frontLayerChanged, this,
        [upd](auto, auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::layerAdded, this,
        [upd](auto, auto) { upd(); }));
    m_rackConns.push_back(connect(
        &first, &Scenario::IntervalModel::layerRemoved, this,
        [upd](auto, auto) { upd(); }));
  }

  const auto& startTsId = m_model.startTimeSyncId();
  const auto& endTsId = m_model.endTimeSyncId();

  // Use ordered intervals so section presenters are created left-to-right,
  // although position is set by date so order only affects zValue stacking.
  for(const auto& itvId : m_model.orderedIntervals())
  {
    const auto& itv = m_model.intervals.at(itvId);

    // Section intervals span from one boundary IS to the next.
    // All are non-boundary by construction (boundary ISes are the
    // start/end timeSyncs — section intervals connect intermediate ISes
    // or the boundaries themselves, but never skip them).
    // handles = true: slot footers are draggable (vertical resize) and slot
    // headers allow switching / moving processes.
    auto* pres = new Scenario::TemporalIntervalPresenter{
        m_zoom, itv, m_context.context, true, &m_view, this};
    pres->on_zoomRatioChanged(m_zoom);
    m_sectionPresenters.append(pres);
  }

  updateSectionLayout();
  updateRowPorts();
}

void SequencePresenter::updateRowPorts()
{
  qDeleteAll(m_rowPorts);
  m_rowPorts.clear();

  const auto ord = m_model.orderedIntervals();
  if(ord.empty())
    return;
  const auto& first = m_model.intervals.at(ord.front());

  auto& portFactory = m_context.context.app.interfaces<Process::PortFactoryList>();

  // Row layout mirrors TemporalIntervalPresenter::updatePositions:
  // each slot is [header][content][footer], stacked from y = 1.
  qreal y = SequenceView::RailHeight + 1.;
  for(const auto& slot : first.smallView())
  {
    const qreal headerY = y;
    y += Scenario::SlotHeader::headerHeight() + slot.height
         + Scenario::SlotFooter::footerHeight();

    if(!slot.frontProcess)
      continue;
    auto pit = first.processes.find(*slot.frontProcess);
    if(pit == first.processes.end())
      continue;

    State::AddressAccessor addr;
    if(auto* a = qobject_cast<const Automation::ProcessModel*>(&*pit))
      addr = a->address();
    else if(auto* g = qobject_cast<const Gradient::ProcessModel*>(&*pit))
      addr = g->address();
    else
      continue;

    // The sequence-level outlet for this row's parameter
    const QString label = addr.toString();
    for(const auto& outlet : m_model.paramOutlets())
    {
      if(outlet->name() == label)
      {
        if(auto fact = portFactory.get(outlet->concreteKey()))
        {
          auto& port = const_cast<Process::ValueOutlet&>(*outlet);
          if(auto* item = fact->makePortItem(port, m_context.context, &m_view, this))
          {
            item->setPos(2., headerY + 2.);
            item->setZValue(11.);
            m_rowPorts.push_back(item);
          }
        }
        break;
      }
    }
  }
}

void SequencePresenter::updateSectionLayout()
{
  if(m_zoom <= 0)
    return;

  for(auto* pres : m_sectionPresenters)
  {
    const auto& itv = pres->model();
    const double x = itv.date().toPixels(m_zoom);
    const double w = itv.duration.defaultDuration().toPixels(m_zoom);
    pres->view()->setPos(x, SequenceView::RailHeight);
    pres->view()->setDefaultWidth(w);
    pres->view()->setMinWidth(itv.duration.minDuration().toPixels(m_zoom));
    pres->view()->setMaxWidth(
        itv.duration.isMaxInfinite(),
        itv.duration.isMaxInfinite() ? -1.
                                     : itv.duration.maxDuration().toPixels(m_zoom));
  }
}

void SequencePresenter::updateHandles()
{
  if(m_zoom <= 0)
    return;

  const auto& startTsId = m_model.startTimeSyncId();
  const auto& endTsId = m_model.endTimeSyncId();

  QVector<SequenceView::HandleData> handles;
  for(const auto& ts : m_model.timeSyncs)
  {
    // Skip boundary timeSyncs — they map to the parent scenario's states
    if(ts.id() == startTsId || ts.id() == endTsId)
      continue;
    const double x = ts.date().toPixelsRaw(m_zoom);
    handles.push_back({ts.id(), x});
  }

  m_view.setHandles(handles);
}

} // namespace Sequence
