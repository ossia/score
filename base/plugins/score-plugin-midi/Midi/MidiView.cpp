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

double View::defaultWidth() const
{
  return m_defaultW;
}

void View::setDefaultWidth(double w)
{
  m_defaultW = w;
  update();
  for(auto cld : childItems())
    cld->update();
}

void View::setRange(int min, int max)
{
  m_min = min;
  m_max = max;
  update();
}

void View::paint_impl(QPainter* p) const
{
  static const MidiStyle style;
  p->setRenderHint(QPainter::Antialiasing, false);
  p->setPen(style.darkPen);
  //    1 3   6 8 10
  //   0 2 4 5 7 9  11
  auto rect = boundingRect();
  auto note_height = rect.height() / (m_max - m_min);

  p->setPen(style.darkerBrush.color());
  p->setBrush(style.darkerBrush);
  p->setPen(style.darkPen);

  if(note_height > 3)
  {
    p->setBrush(style.lightBrush);
    for (int i = 1; i < (m_max - m_min); i++)
    {
      switch (i % 12)
      {
        case 0: case 2: case 4: case 5: case 7: case 9: case 11:
        {
          p->drawRect(QRectF{0., rect.height() - note_height * i - 1, rect.width(), note_height});
          break;
        }
      }
    }

    p->setBrush(style.darkBrush);
    for (int i = 0; i < (m_max - m_min) + 1; i++)
    {
      switch (i % 12)
      {
        case 1: case 3: case 6: case 8: case 10:
        {
          p->drawRect(QRectF{0., rect.height() - note_height * i - 1, rect.width(), note_height});
          break;
        }
      }
    }

    p->setPen(style.darkerBrush.color());
    static constexpr const char* texts[]{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    for (int i = 1; i < (m_max - m_min); i++)
    {
      switch (i % 12)
      {
        case 0: case 2: case 4: case 5: case 7: case 9: case 11:
        {
          p->drawText(
                QRectF{2., rect.height() - note_height * i - 1, rect.width(), note_height},
                texts[i%12],
              QTextOption{Qt::AlignVCenter});
          break;
        }
      }
    }

    for (int i = 0; i < (m_max - m_min) + 1; i++)
    {
      switch (i % 12)
      {
        case 1: case 3: case 6: case 8: case 10:
        {
          p->drawText(
                QRectF{2., rect.height() - note_height * i - 1, rect.width(), note_height},
                texts[i%12],
              QTextOption{Qt::AlignVCenter});
          break;
        }
      }
    }
  }

  if (!m_selectArea.isEmpty())
  {
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

NoteData View::noteAtPos(QPointF point) const
{
  const auto rect = boundingRect();

  NoteData n;
  n.m_start = std::max(0., point.x() / m_defaultW);
  n.m_duration = 0.1;
  n.m_pitch = qBound(
      m_min,
      1 + int(m_max
          - (qMin(rect.bottom(), qMax(point.y(), rect.top())) / rect.height())
                * (m_max - m_min)),
      m_max);
  n.m_velocity = 127.;
  return n;
}
}
