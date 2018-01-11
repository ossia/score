// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiView.hpp"
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QPainter>
#include <Midi/MidiStyle.hpp>
namespace Midi
{

View::View(QGraphicsItem* parent) : Process::LayerView{parent}
{
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
  this->setFlag(QGraphicsItem::ItemIsFocusable, true);
}

View::~View()
{
}

void View::paint_impl(QPainter* p) const
{
  static const MidiStyle style;
  p->setRenderHint(QPainter::Antialiasing, false);
  p->setPen(style.darkPen);
  //    1 3   6 8 10
  //   0 2 4 5 7 9  11
  auto rect = boundingRect();
  auto note_height = rect.height() / 127.;

  if(note_height > 3)
  {
    p->setBrush(style.lightBrush);
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
          p->drawRect(QRectF{0., rect.height() - note_height * i - 1, rect.width(), note_height});
          break;
        }

        default:
          break;
      }
    }

    p->setBrush(style.darkBrush);
    for (int i = 0; i < 128; i++)
    {
      switch (i % 12)
      {
        case 1:
        case 3:
        case 6:
        case 8:
        case 10:
        {
          p->drawRect(QRectF{0., rect.height() - note_height * i - 1, rect.width(), note_height});
          break;
        }
        default:
          break;
      }
    }

  }

  if (!m_selectArea.isEmpty())
  {
    p->setCompositionMode(QPainter::CompositionMode_Xor);
    p->setBrush(style.transparentBrush);
    p->setPen(style.selectionPen);
    p->drawPath(m_selectArea);
  }
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  emit askContextMenu(event->screenPos(), event->scenePos());
  event->accept();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  emit pressed(ev->scenePos());

  ev->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  QPainterPath p;
  p.addRect(QRectF{ev->buttonDownPos(Qt::LeftButton), ev->pos()});
  this->scene()->setSelectionArea(mapToScene(p));

  m_selectArea = p;
  update();
  ev->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  m_selectArea = {};
  update();
  ev->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* ev)
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
