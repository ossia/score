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
static const MidiStyle style;

void View::paint_impl(QPainter* p) const
{
  p->setRenderHint(QPainter::Antialiasing, false);
  p->setPen(style.darkPen);
  //    1 3   6 8 10
  //   0 2 4 5 7 9  11
  const auto rect = boundingRect();
  const auto note_height = rect.height() / (m_max - m_min);

  const auto for_white_notes = [&] (auto fun) {
    for (int i = m_min + 1; i <= m_max; i++)
      switch (i % 12)
      {
        case 0: case 2: case 4: case 5: case 7: case 9: case 11:
          fun(i);
          break;
      }
  };
  const auto for_black_notes = [&] (auto fun) {
    for (int i = m_min + 1; i <= m_max; i++)
      switch (i % 12)
      {
        case 1: case 3: case 6: case 8: case 10:
          fun(i);
          break;
      }
  };


  p->setPen(style.darkerBrush.color());
  p->setBrush(style.darkerBrush);
  p->setPen(style.darkPen);

  if(note_height > 3)
  {
    const auto draw_bg = [&] (int i) {
      p->drawRect(QRectF{0., rect.height() + note_height * (m_min - i) - 1, rect.width(), note_height});
    };

    p->setBrush(style.lightBrush);
    for_white_notes(draw_bg);

    p->setBrush(style.darkBrush);
    for_black_notes(draw_bg);

    if(note_height > 10)
    {
      static constexpr const char* texts[]{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      const auto draw_text = [&] (int i) {
        p->drawText(
              QRectF{2., rect.height() + note_height * (m_min - i ) - 1, rect.width(), note_height},
              texts[i%12],
            QTextOption{Qt::AlignVCenter});
      };

      p->setPen(style.darkerBrush.color());
      for_white_notes(draw_text);
      for_black_notes(draw_text);
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
  qDebug() << n.m_pitch;
  n.m_velocity = 127.;
  return n;
}
}
