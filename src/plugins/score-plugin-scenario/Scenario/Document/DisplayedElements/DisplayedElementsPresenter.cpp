// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DisplayedElementsPresenter.hpp"

#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <Scenario/Document/TimeSync/TriggerView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/graphics/GraphicsProxyObject.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QGuiApplication>

#include <Magnetism/MagnetismAdjuster.hpp>
#include <wobjectimpl.h>

#include <tuple>

#include <type_traits>
W_OBJECT_IMPL(Scenario::DisplayedElementsPresenter)
namespace Scenario
{
class DisplayedElementsModel;

DisplayedElementsPresenter::DisplayedElementsPresenter(ScenarioDocumentPresenter& parent)
    : QObject{&parent}
    , BaseScenarioPresenter<
          DisplayedElementsModel,
          FullViewIntervalPresenter>{parent.displayedElements}
    , m_model{parent}
{
}

DisplayedElementsPresenter::~DisplayedElementsPresenter()
{
  disconnect(
      &m_model.context().execTimer,
      &QTimer::timeout,
      this,
      &DisplayedElementsPresenter::on_intervalExecutionTimer);

  // TODO use directly displayedelementspresentercontainer
  delete m_intervalPresenter;
  delete m_startStatePresenter;
  delete m_endStatePresenter;
  delete m_startEventPresenter;
  delete m_endEventPresenter;
  delete m_startNodePresenter;
  delete m_endNodePresenter;
}

BaseGraphicsObject& DisplayedElementsPresenter::view() const
{
  return m_model.view().baseItem();
}

void DisplayedElementsPresenter::on_displayedIntervalChanged(const IntervalModel& m)
{
  remove();

  // Create states / events
  auto& ctx = m_model.context();
  auto& provider = ctx.app.interfaces<DisplayedElementsProviderList>();
  DisplayedElementsPresenterContainer elts = provider.make(
      &DisplayedElementsProvider::make_presenters, m, ctx, &m_model.view().baseItem(), this);
  m_intervalPresenter = elts.interval;
  m_startStatePresenter = elts.startState;
  m_endStatePresenter = elts.endState;
  m_startEventPresenter = elts.startEvent;
  m_endEventPresenter = elts.endEvent;
  m_startNodePresenter = elts.startNode;
  m_endNodePresenter = elts.endNode;

  m_connections.push_back(
      con(m_intervalPresenter->model().duration,
          &IntervalDurations::defaultDurationChanged,
          this,
          &DisplayedElementsPresenter::on_displayedIntervalDurationChanged));
  m_connections.push_back(
      con(m_intervalPresenter->model(), &IntervalModel::heightFinishedChanging, this, [&]() {
        on_displayedIntervalHeightChanged(m_intervalPresenter->view()->height());
      }));

  m_connections.push_back(
      connect(m_intervalPresenter, &FullViewIntervalPresenter::heightChanged, this, [&]() {
        on_displayedIntervalHeightChanged(m_intervalPresenter->view()->height());
      }));

  auto elements = std::make_tuple(
      m_intervalPresenter,
      m_startStatePresenter,
      m_endStatePresenter,
      m_startEventPresenter,
      m_endEventPresenter,
      m_startNodePresenter,
      m_endNodePresenter);

  ossia::for_each_in_tuple(elements, [&](auto elt) {
    using elt_t = std::remove_reference_t<decltype(*elt)>;
    m_connections.push_back(
        connect(elt, &elt_t::pressed, &m_model, &ScenarioDocumentPresenter::pressed));
    m_connections.push_back(
        connect(elt, &elt_t::moved, &m_model, &ScenarioDocumentPresenter::moved));
    m_connections.push_back(
        connect(elt, &elt_t::released, &m_model, &ScenarioDocumentPresenter::released));
  });

  elts.startState->view()->disableOverlay();
  elts.endState->view()->disableOverlay();

  auto& magnetismHandler = (Process::MagnetismAdjuster&)m_model.context()
                               .app.interfaces<Process::MagnetismAdjuster>();
  magnetismHandler.registerHandler(*m_intervalPresenter);

  showInterval();

  on_zoomRatioChanged(m.zoom());

  con(ctx.execTimer,
      &QTimer::timeout,
      this,
      &DisplayedElementsPresenter::on_intervalExecutionTimer);
}

void DisplayedElementsPresenter::showInterval()
{
  // We set the focus on the main scenario.
  auto& rack = m_intervalPresenter->getSlots();
  if (!rack.empty())
  {
    const auto& front = rack.front();
    if(auto* slot = front.getLayerSlot())
    {
      auto& procs = slot->layers;
      if (!procs.empty())
        requestFocusedPresenterChange(procs.front().mainPresenter());
    }
    // TODO else ??
  }

  m_intervalPresenter->updateHeight();
}

void DisplayedElementsPresenter::on_zoomRatioChanged(ZoomRatio r)
{
  if (!m_intervalPresenter)
    return;
  updateLength(m_intervalPresenter->model().duration.defaultDuration().toPixels(r));

  m_intervalPresenter->on_zoomRatioChanged(r);
}

void DisplayedElementsPresenter::on_displayedIntervalDurationChanged(TimeVal t)
{
  updateLength(t.toPixels(m_model.zoomRatio()));
}

const double deltaX = 10.;
const double deltaY = 45.;
void DisplayedElementsPresenter::on_displayedIntervalHeightChanged(double size)
{
  auto cur_rect = m_model.view().view().sceneRect();
  QRectF new_rect{
      qreal(ScenarioLeftSpace),
      0.,
      m_intervalPresenter->model().duration.guiDuration().toPixels(
          m_intervalPresenter->zoomRatio()),
      size + 40};

  if (qApp->mouseButtons() & Qt::MouseButton::LeftButton)
    new_rect.setHeight(std::max(new_rect.height(), cur_rect.height()));
  m_model.updateRect(new_rect);

  const double Y = m_model.isNodal() ? 20 : deltaY;
  m_startEventPresenter->view()->setPos(deltaX, Y);
  m_startNodePresenter->view()->setPos(deltaX, Y);
  m_startStatePresenter->view()->setPos(deltaX, Y);
  m_intervalPresenter->view()->setPos(deltaX, Y);

  m_startEventPresenter->view()->setExtent({0., 1.});
  m_startNodePresenter->view()->setExtent({0., (qreal)size});
  m_endEventPresenter->view()->setExtent({0., 1.});
  m_endNodePresenter->view()->setExtent({0., size});

  m_startEventPresenter->view()->setZValue(0);
  m_startNodePresenter->view()->setZValue(0);
  m_endEventPresenter->view()->setZValue(0);
  m_endNodePresenter->view()->setZValue(0);
  m_intervalPresenter->view()->setZValue(1);
  m_startStatePresenter->view()->setZValue(2);
}

void DisplayedElementsPresenter::recomputeHeight()
{
  const double rightX = m_endStatePresenter->view()->x();
  const double Y = deltaY;

  m_startEventPresenter->view()->setPos(deltaX, Y);
  m_startNodePresenter->view()->setPos(deltaX, Y);
  m_startStatePresenter->view()->setPos(deltaX, Y);
  m_intervalPresenter->view()->setPos(deltaX, Y);

  m_endStatePresenter->view()->setPos({rightX, Y});
  m_endEventPresenter->view()->setPos({rightX, Y});
  m_endNodePresenter->view()->setPos({rightX, Y});
}

void DisplayedElementsPresenter::setVisible(bool b)
{
  m_startEventPresenter->view()->setVisible(b);
  m_startNodePresenter->view()->setVisible(b);
  m_startStatePresenter->view()->setVisible(b);
  m_intervalPresenter->view()->setVisible(b);
  m_endStatePresenter->view()->setVisible(b);
  m_endEventPresenter->view()->setVisible(b);
  m_endNodePresenter->view()->setVisible(b);
}

void DisplayedElementsPresenter::remove()
{
  auto& magnetismHandler = (Process::MagnetismAdjuster&)m_model.context()
                               .app.interfaces<Process::MagnetismAdjuster>();
  magnetismHandler.unregisterHandler(m_intervalPresenter);

  disconnect(
      &m_model.context().execTimer,
      &QTimer::timeout,
      this,
      &DisplayedElementsPresenter::on_intervalExecutionTimer);

  for (auto& con : m_connections)
    QObject::disconnect(con);

  m_connections.clear();
  // TODO use directly displayedelementspresentercontainer
  delete m_intervalPresenter;
  m_intervalPresenter = nullptr;
  delete m_startStatePresenter;
  m_startStatePresenter = nullptr;
  delete m_endStatePresenter;
  m_endStatePresenter = nullptr;
  delete m_startEventPresenter;
  m_startEventPresenter = nullptr;
  delete m_endEventPresenter;
  m_endEventPresenter = nullptr;
  delete m_startNodePresenter;
  m_startNodePresenter = nullptr;
  delete m_endNodePresenter;
  m_endNodePresenter = nullptr;
}

void DisplayedElementsPresenter::setSnapLine(TimeVal t, bool enabled)
{

}

void DisplayedElementsPresenter::updateLength(double length)
{
  const double Y = m_model.isNodal() ? 20 : deltaY;
  // TODO why isn't rect updated here.
  m_endStatePresenter->view()->setPos({deltaX + length, Y});
  m_endEventPresenter->view()->setPos({deltaX + length, Y});
  m_endNodePresenter->view()->setPos({deltaX + length, Y});
}

void DisplayedElementsPresenter::on_intervalExecutionTimer()
{
  auto& cst = *m_intervalPresenter;
  auto pp = cst.model().duration.playPercentage();
  if (double w = cst.on_playPercentageChanged(pp))
  {
    auto& v = *cst.view();
    const auto r = v.boundingRect();

    if (pp != 0)
      v.update(r.x() + v.playWidth() - w, r.y(), 2. * w, 5.);
    else
      v.update();
  }
}
}
