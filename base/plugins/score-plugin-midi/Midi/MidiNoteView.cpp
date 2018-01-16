// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiNoteView.hpp"
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <Midi/MidiStyle.hpp>
#include <Midi/MidiView.hpp>
namespace Midi
{

NoteView::NoteView(const Note& n, View* parent)
    : QGraphicsItem{parent}, note{n}
{
  this->setFlag(QGraphicsItem::ItemIsSelectable, true);
  this->setFlag(QGraphicsItem::ItemIsMovable, true);
  this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
  this->setAcceptHoverEvents(true);
}

void NoteView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static MidiStyle s;
  painter->setRenderHint(QPainter::Antialiasing, false);

  painter->setBrush(this->isSelected() ? s.noteSelectedBaseBrush : s.noteBaseBrush);
  painter->setPen(s.noteBasePen);
  painter->drawRect(boundingRect().adjusted(0., -1., 0., -1.));

  if(m_height > 6)
  {
    auto orange = s.noteBaseBrush.color();
    orange.setHslF(
        orange.hslHueF() - 0.02, 1., 0.25 + (127. - note.velocity()) / 256.);

    s.paintedNote.setColor(orange);

    painter->setPen(s.paintedNote);
    painter->drawLine(
          QPointF{2., m_height / 2.}, QPointF{m_width - 2., m_height / 2.});
  }
}

QVariant NoteView::itemChange(
    QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
    case QGraphicsItem::ItemPositionChange:
    {
      QPointF newPos = value.toPointF();
      QRectF rect = this->parentItem()->boundingRect();
      auto height = rect.height();
      auto note_height = height / 127.;

      newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
      // Snap to grid : we round y to the closest multiple of 127
      int note = qBound(
          0,
          int(127
              - (qMin(rect.bottom(), qMax(newPos.y(), rect.top())) / height)
                    * 127),
          127);

      newPos.setY(height - note * note_height);
      return newPos;
    }
    case QGraphicsItem::ItemSelectedChange:
    {
      this->setZValue(10 * (int)value.toBool());
      break;
    }
    default:
      break;
  }

  return QGraphicsItem::itemChange(change, value);
}

void NoteView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  if (event->pos().x() >= this->boundingRect().width() - 2)
  {
    this->setCursor(Qt::SplitHCursor);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }

  QGraphicsItem::hoverEnterEvent(event);
}

void NoteView::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  if (event->pos().x() >= this->boundingRect().width() - 2)
  {
    this->setCursor(Qt::SplitHCursor);
  }
  else
  {
    this->setCursor(Qt::ArrowCursor);
  }

  QGraphicsItem::hoverMoveEvent(event);
}

void NoteView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setCursor(Qt::ArrowCursor);

  QGraphicsItem::hoverEnterEvent(event);
}

void NoteView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->pos().x() >= this->boundingRect().width() - 2)
  {
    m_scaling = true;
    event->accept();
  }
  else
  {
    m_scaling = false;
    QGraphicsItem::mousePressEvent(event);
  }
}

void NoteView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_scaling)
  {
    this->setWidth(std::max(2., event->pos().x()));
    event->accept();
  }
  else
  {
    QGraphicsItem::mouseMoveEvent(event);
  }
}

void NoteView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_scaling)
  {
    emit noteScaled(m_width / ((View*)parentItem())->defaultWidth());
    event->accept();
  }
  else
  {
    emit noteChangeFinished();
    QGraphicsItem::mouseReleaseEvent(event);
  }
}
}
