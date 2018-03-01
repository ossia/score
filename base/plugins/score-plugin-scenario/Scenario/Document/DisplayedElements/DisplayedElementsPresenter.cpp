// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QApplication>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <score/tools/std/Optional.hpp>

#include "DisplayedElementsPresenter.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalView.hpp>
#include <Scenario/Document/Interval/FullView/FullViewIntervalPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Event/EventView.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/State/StateView.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncView.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/IdentifiedObjectMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/Todo.hpp>
#include <score/widgets/GraphicsProxyObject.hpp>
#include <tuple>
#include <type_traits>

namespace Scenario
{
class DisplayedElementsModel;

DisplayedElementsPresenter::DisplayedElementsPresenter(
    ScenarioDocumentPresenter& parent)
    : QObject{&parent}
    , BaseScenarioPresenter<DisplayedElementsModel, FullViewIntervalPresenter>{
        parent.displayedElements}
    , m_model{parent}
{
}

DisplayedElementsPresenter::~DisplayedElementsPresenter()
{
  disconnect(
      &m_model.context().execTimer, &QTimer::timeout, this,
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

void DisplayedElementsPresenter::on_displayedIntervalChanged(
    const IntervalModel& m)
{
  disconnect(
      &m_model.context().execTimer, &QTimer::timeout, this,
      &DisplayedElementsPresenter::on_intervalExecutionTimer);

  for (auto& con : m_connections)
    QObject::disconnect(con);

  m_connections.clear();
  // TODO use directly displayedelementspresentercontainer
  delete m_intervalPresenter;
  delete m_startStatePresenter;
  delete m_endStatePresenter;
  delete m_startEventPresenter;
  delete m_endEventPresenter;
  delete m_startNodePresenter;
  delete m_endNodePresenter;

  // Create states / events
  auto& ctx = m_model.context();
  auto& provider = ctx.app.interfaces<DisplayedElementsProviderList>();
  DisplayedElementsPresenterContainer elts = provider.make(
      &DisplayedElementsProvider::make_presenters, m, ctx,
      &m_model.view().baseItem(), this);
  m_intervalPresenter = elts.interval;
  m_startStatePresenter = elts.startState;
  m_endStatePresenter = elts.endState;
  m_startEventPresenter = elts.startEvent;
  m_endEventPresenter = elts.endEvent;
  m_startNodePresenter = elts.startNode;
  m_endNodePresenter = elts.endNode;

  m_connections.push_back(
      con(m_intervalPresenter->model().duration,
          &IntervalDurations::defaultDurationChanged, this,
          &DisplayedElementsPresenter::on_displayedIntervalDurationChanged));
  m_connections.push_back(
        con(m_intervalPresenter->model(),
            &IntervalModel::heightFinishedChanging, this,
            [&]() {
              on_displayedIntervalHeightChanged(
                  m_intervalPresenter->view()->height());
            }));

  m_connections.push_back(connect(
      m_intervalPresenter, &FullViewIntervalPresenter::heightChanged, this,
      [&]() {
        on_displayedIntervalHeightChanged(
            m_intervalPresenter->view()->height());
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
    m_connections.push_back(connect(
        elt, &elt_t::pressed, &m_model, &ScenarioDocumentPresenter::pressed));
    m_connections.push_back(connect(
        elt, &elt_t::moved, &m_model, &ScenarioDocumentPresenter::moved));
    m_connections.push_back(connect(
        elt, &elt_t::released, &m_model, &ScenarioDocumentPresenter::released));
  });

  elts.startState->view()->disableOverlay();
  elts.endState->view()->disableOverlay();

  showInterval();

  on_zoomRatioChanged(m_intervalPresenter->zoomRatio());

  con(ctx.execTimer, &QTimer::timeout, this,
      &DisplayedElementsPresenter::on_intervalExecutionTimer);
}

void DisplayedElementsPresenter::showInterval()
{
  // We set the focus on the main scenario.
  auto& rack = m_intervalPresenter->getSlots();
  if (!rack.empty())
  {
    requestFocusedPresenterChange(rack.front().process.presenter);
  }

  m_intervalPresenter->updateHeight();
}

void DisplayedElementsPresenter::on_zoomRatioChanged(ZoomRatio r)
{
  if (!m_intervalPresenter)
    return;
  updateLength(m_intervalPresenter->model()
                   .duration.defaultDuration()
                   .toPixels(r));

  m_intervalPresenter->on_zoomRatioChanged(r);
}

void DisplayedElementsPresenter::on_displayedIntervalDurationChanged(
    TimeVal t)
{
  updateLength(t.toPixels(m_model.zoomRatio()));
}

const double deltaX = 10.;
const double deltaY = 20.;
void DisplayedElementsPresenter::on_displayedIntervalHeightChanged(
    double size)
{
  auto cur_rect = m_model.view().view().sceneRect();
  QRectF new_rect{qreal(ScenarioLeftSpace), 0.,
                  m_intervalPresenter->model()
                      .duration.guiDuration()
                      .toPixels(m_intervalPresenter->zoomRatio()),
                  size + 40};

  if(qApp->mouseButtons() & Qt::MouseButton::LeftButton)
    new_rect.setHeight(std::max(new_rect.height(), cur_rect.height()));
  m_model.updateRect(new_rect);

  m_startEventPresenter->view()->setPos(deltaX, deltaY);
  m_startNodePresenter->view()->setPos(deltaX, deltaY);
  m_startStatePresenter->view()->setPos(deltaX, deltaY);
  m_intervalPresenter->view()->setPos(deltaX, deltaY);

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

void DisplayedElementsPresenter::updateLength(double length)
{
  // TODO why isn't rect updated here.
  m_endStatePresenter->view()->setPos({deltaX + length, deltaY});
  m_endEventPresenter->view()->setPos({deltaX + length, deltaY});
  m_endNodePresenter->view()->setPos({deltaX + length, deltaY});
}

void DisplayedElementsPresenter::on_intervalExecutionTimer()
{
  auto& cst = *m_intervalPresenter;
  auto pp = cst.model().duration.playPercentage();
  if(double w = cst.on_playPercentageChanged(pp))
  {
    auto& v = *cst.view();
    const auto r = v.boundingRect();

    if(pp != 0)
      v.update(r.x() + v.playWidth() - w, r.y(), 2. * w, 5.);
    else
      v.update();
  }
}
}
