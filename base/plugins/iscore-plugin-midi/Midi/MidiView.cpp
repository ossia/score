#include "MidiView.hpp"
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QQuickWidget>
#include <QKeyEvent>
#include <QPainter>

namespace Midi
{

View::View(QQuickPaintedItem* parent) : Process::LayerView{parent}
{
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
  this->setFlag(QQuickPaintedItem::ItemIsFocusable, true);
}

View::~View()
{
}

void View::paint_impl(QPainter* p) const
{
  p->setRenderHint(QPainter::Antialiasing, false);
  //    1 3   6 8 10
  //   0 2 4 5 7 9  11
  QColor l1 = QColor::fromRgb(200, 200, 200);
  QColor d1 = QColor::fromRgb(170, 170, 170);
  QColor d2 = d1.darker();
  p->setPen(d2);
  auto rect = boundingRect();
  auto note_height = rect.height() / 127.;

  for (int i = 0; i < 128; i++)
  {
    switch (i % 12)
    {
      case 0:
      case 2:
      case 4:
      case 5:
      case 7:
      case 9:
      case 11:
      {
        p->setBrush(l1);
        break;
      }

      case 1:
      case 3:
      case 6:
      case 8:
      case 10:
      {
        p->setBrush(d1);
        break;
      }
    }

    p->drawRect(0, rect.height() - note_height * i, rect.width(), note_height);
  }

  if (!m_selectArea.isEmpty())
  {
    p->setCompositionMode(QPainter::CompositionMode_Xor);
    p->setBrush(Qt::transparent);
    p->setPen(QPen{QColor{0, 0, 0, 127}, 2, Qt::DashLine, Qt::SquareCap,
                   Qt::BevelJoin});

    p->drawPath(m_selectArea);
  }
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), event->scenePos());
  event->accept();
}

void View::mousePressEvent(QMouseEvent* ev)
{
  emit pressed();

  ev->accept();
}

void View::mouseMoveEvent(QMouseEvent* ev)
{
  QPainterPath p;
  p.addRect(QRectF{ev->buttonDownPos(Qt::LeftButton), ev->pos()});
  this->scene()->setSelectionArea(mapToScene(p));

  m_selectArea = p;
  update();
  ev->accept();
}

void View::mouseReleaseEvent(QMouseEvent* ev)
{
  m_selectArea = {};
  update();
  ev->accept();
}

void View::mouseDoubleClickEvent(QMouseEvent* ev)
{
  emit doubleClicked(ev->pos());
  ev->accept();
}

void View::keyPressEvent(QKeyEvent* ev)
{
  if (ev->key() == Qt::Key_Backspace || ev->key() == Qt::Key_Delete)
  {
    emit deleteRequested();
  }

  ev->accept();
}

NoteData noteAtPos(QPointF point, const QRectF& rect)
{
  NoteData n;
  n.m_start = qBound(0., point.x() / rect.width(), 1.);
  n.m_duration = 0.1;
  n.m_pitch = qBound(
      0,
      int(127
          - (qMin(rect.bottom(), qMax(point.y(), rect.top())) / rect.height())
                * 127),
      127);
  n.m_velocity = 127.;
  return n;
}
}
