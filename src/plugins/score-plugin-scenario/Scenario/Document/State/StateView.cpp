// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateView.hpp"

#include "StateMenuOverlay.hpp"
#include "StatePresenter.hpp"

#include <Process/ProcessMimeSerialization.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Application/Menus/ScenarioCopy.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

#include <QApplication>
#include <QCursor>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QScreen>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::StateView)
namespace Scenario
{
static const QPainterPath smallNonDilated{[] {
  QPainterPath p;
  p.addEllipse({0, 0}, StateView::pointRadius, StateView::pointRadius);
  return p;
}()};
static const QPainterPath fullNonDilated{[] {
  QPainterPath p;
  p.addEllipse({0, 0}, StateView::fullRadius, StateView::fullRadius);
  return p;
}()};
static const QPainterPath smallDilated{[] {
  QPainterPath p;
  p.addEllipse(
      {0, 0},
      StateView::pointRadius * StateView::dilated,
      StateView::pointRadius * StateView::dilated);
  return p;
}()};
static const QPainterPath fullDilated{[] {
  QPainterPath p;
  p.addEllipse(
      {0, 0},
      StateView::fullRadius * StateView::dilated,
      StateView::fullRadius * StateView::dilated);
  return p;
}()};
bool is_hidpi()
{
  static const bool res = (qApp->screens().front()->devicePixelRatio() > 1.5);
  return res;
}
StateView::StateView(StatePresenter& pres, QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_presenter{pres}
    , m_dilated{}
    , m_containMessage{}
    , m_selected{}
    , m_hovered{}
    , m_hasOverlay{true}
    , m_moving{}
{
  if (!is_hidpi())
    this->setCacheMode(QGraphicsItem::CacheMode::ItemCoordinateCache);
  this->setParentItem(parent);

  this->setCursor(QCursor(Qt::CrossCursor));
  this->setZValue(ZPos::State);
  this->setAcceptDrops(true);
  this->setAcceptHoverEvents(true);
}

QRectF StateView::boundingRect() const
{
  const auto radius = m_dilated ? fullRadius * dilated : fullRadius;
  return {-radius, -radius, 2. * radius, 2. * radius};
}

void StateView::paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, true);
  auto& skin = Process::Style::instance();
  painter->setPen(skin.NoPen());

  if (m_containMessage)
  {
    painter->setBrush(skin.StateOutline());
    if (m_dilated)
      painter->drawPath(fullDilated);
    else
      painter->drawPath(fullNonDilated);
  }

  auto& brush =
      (m_status.get() != ExecutionStatus::Editing)
      ? m_status.stateStatusColor(skin)
      : (m_selected ? skin.StateSelected() : skin.StateDot());

  painter->setBrush(brush);
  painter->setPen(skin.StateTemporalPointPen(brush));
  if (m_dilated)
    painter->drawPath(smallDilated);
  else
    painter->drawPath(smallNonDilated);

#if defined(SCORE_SCENARIO_DEBUG_RECTS)
  painter->setBrush(Qt::NoBrush);
  painter->setPen(Qt::darkYellow);
  painter->drawRect(boundingRect());
#endif

  painter->setRenderHint(QPainter::Antialiasing, false);
}

void StateView::setContainMessage(bool arg)
{
  m_containMessage = arg;
  update();
}

void StateView::setSelected(bool b)
{
  m_selected = b;
  setDilatation(m_selected);
  setZValue(m_selected ? ZPos::SelectedState : ZPos::State);

  updateOverlay();
}

void StateView::setStatus(ExecutionStatus status)
{
  if (m_status.get() == status)
    return;
  m_status.set(status);
  update();
}

void StateView::disableOverlay()
{
  m_hasOverlay = false;
}

void StateView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_moving = false;
  if (event->button() == Qt::MouseButton::LeftButton)
    m_presenter.pressed(event->scenePos());
  event->accept();
}

void StateView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->buttons() & Qt::MiddleButton)
  {
    if (auto si = dynamic_cast<Scenario::ScenarioInterface*>(
            presenter().model().parent()))
    {
      auto obj = copySelectedElementsToJson(
          *const_cast<ScenarioInterface*>(si),
          score::IDocument::documentContext(*m_presenter.model().parent()));

      if (!obj.empty())
      {
        QDrag d{this};
        auto m = new QMimeData;
        QJsonDocument doc{obj};
        ;
        m->setData(
            score::mime::scenariodata(), doc.toJson(QJsonDocument::Indented));
        d.setMimeData(m);
        d.exec();
      }
    }
  }
  if (m_moving
      || (event->buttonDownScreenPos(Qt::LeftButton) - event->screenPos())
                 .manhattanLength()
             > QApplication::startDragDistance())
  {
    m_moving = true;
    m_presenter.moved(event->scenePos());
  }
  event->accept();
}

void StateView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_moving = false;
  m_presenter.released(event->scenePos());
  event->accept();
}

void StateView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = true;
  updateOverlay();
  event->accept();
}

void StateView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = false;
  updateOverlay();
  event->accept();
}

void StateView::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
  setDilatation(true);
}

void StateView::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
  setDilatation(m_selected);
}

void StateView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(*event->mimeData());
  setDilatation(m_selected);
}

void StateView::setDilatation(bool val)
{
  prepareGeometryChange();
  m_dilated = val;
  this->update();
}

void StateView::updateOverlay()
{
  if (m_hasOverlay)
  {
    if (m_selected || m_hovered)
    {
      if(m_overlay)
        return;

      m_overlay = new StatePlusOverlay{this};
      m_overlay->setPos(0, -14);

      m_graphOverlay = new StateGraphPlusOverlay{this};
      m_graphOverlay->setPos(0, 10);
    }
    else
    {
      delete m_overlay;
      m_overlay = nullptr;
      delete m_graphOverlay;
      m_graphOverlay = nullptr;
    }
  }
}
}
