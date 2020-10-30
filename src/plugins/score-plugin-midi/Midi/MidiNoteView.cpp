// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MidiNoteView.hpp"

#include <Midi/MidiStyle.hpp>
#include <Midi/MidiView.hpp>
#include <Midi/MidiPresenter.hpp>

#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>
namespace Midi
{

NoteView::NoteView(const Note& n, Presenter& p, View* parent)
  : QGraphicsItem{parent}
  , note{n}
  , m_presenter{p}
  , m_action{None}
{
  this->setFlag(QGraphicsItem::ItemIsSelectable, true);
  this->setFlag(QGraphicsItem::ItemIsMovable, true);
  this->setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
  this->setAcceptHoverEvents(true);
}

void NoteView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const MidiStyle s;

  painter->setRenderHint(QPainter::Antialiasing, false);

  if (m_width <= 1.2)
  {
    painter->setPen(this->isSelected() ? s.noteSelectedBasePen : s.noteBasePen);
    painter->drawLine(0, 0, 0, m_height - 1.5);
  }
  else
  {
    painter->setPen(this->isSelected() ? s.noteSelectedBasePen : Qt::NoPen);
    painter->setBrush(s.paintedNoteBrush[note.velocity()]);

    painter->drawRect(boundingRect().adjusted(0., 0., 0., 0));
  }
}

QPointF NoteView::closestPos(QPointF newPos) const noexcept
{
  auto& view = *(View*)parentItem();
  const auto [min, max] = view.range();
  const double count = view.visibleCount();
  const QRectF rect = view.boundingRect();
  const auto height = rect.height();
  const auto note_height = height / count;

  newPos.setX(qMin(rect.right(), qMax(newPos.x(), rect.left())));
  // Snap to grid : we round y to the closest multiple of 127
  auto bounded = (qBound(rect.top(), newPos.y() - note_height * 0.45, rect.bottom())) / height;
  int note = qBound(min, int(max - bounded * count), max);

  newPos.setY(height - (note - min + 1) * note_height);
  return newPos;
}

QRectF NoteView::computeRect() const noexcept
{
  auto& view = *(View*)parentItem();
  const auto h = view.height();
  const auto w = view.defaultWidth();
  const auto [min, max] = view.range();
  const auto note_height = h / view.visibleCount();
  const QRectF rect{
      note.start() * w,
      h - std::ceil((note.pitch() - min + 1) * note_height),
      note.duration() * w,
      note_height};

  return rect;
}

QVariant NoteView::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
  switch (change)
  {
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
    auto& skin = score::Skin::instance();
    this->setCursor(skin.CursorScaleH);
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
    auto& skin = score::Skin::instance();
    this->setCursor(skin.CursorScaleH);
  }
  else
  {
    if(qApp->keyboardModifiers() == Qt::ShiftModifier)
    {
      auto& skin = score::Skin::instance();
      this->setCursor(skin.CursorSpin);
    }
    else
    {
      this->setCursor(Qt::ArrowCursor);
    }
  }

  QGraphicsItem::hoverMoveEvent(event);
}

void NoteView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  this->setCursor(Qt::ArrowCursor);

  QGraphicsItem::hoverEnterEvent(event);
}

bool NoteView::canEdit() const
{
  return boundingRect().height() > 5;
}

static QPointF noteview_origpoint;
void NoteView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto mods = QGuiApplication::keyboardModifiers();
  if (!(mods & Qt::ControlModifier) && !isSelected())
    m_presenter.on_deselectOtherNotes();

  setSelected(true);

  m_action = None;
  if (canEdit())
  {
    if (event->pos().x() >= this->boundingRect().width() - 2)
    {
      m_action = Scale;
    }
    else if (mods & Qt::ShiftModifier)
    {
      m_action = ChangeVelocity;
    }
    else if (mods & Qt::AltModifier)
    {
      m_action = Duplicate;
    }
    else
    {
      m_action = Move;
      noteview_origpoint = this->pos();
    }
  }
  event->accept();
}

void NoteView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (canEdit())
  {
    switch(m_action)
    {
      case Move:
        this->setPos(closestPos(
            noteview_origpoint + event->scenePos() - event->buttonDownScenePos(Qt::LeftButton)));
         m_presenter.on_noteChanged(*this);
        break;
      case Scale:
        this->setWidth(std::max(2., event->pos().x()));
        break;
      case Duplicate:
        m_presenter.on_duplicate();
        break;
      case ChangeVelocity:
        m_presenter.on_requestVelocityChange(note, event->buttonDownScenePos(Qt::LeftButton).y() - event->scenePos().y());
        break;
      case None:
        break;
    }
  }
  event->accept();
}

void NoteView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (canEdit())
  {
    switch(m_action)
    {
      case Move:
        this->setPos(closestPos(
            noteview_origpoint + event->scenePos() - event->buttonDownScenePos(Qt::LeftButton)));
         m_presenter.on_noteChanged(*this);
         m_presenter.on_noteChangeFinished(*this);
        break;
      case Scale:
        this->setWidth(std::max(2., event->pos().x()));
        m_presenter.on_noteScaled(note, m_width / ((View*)parentItem())->defaultWidth());
        break;
      case Duplicate:
        m_presenter.on_duplicate();
        break;
      case ChangeVelocity:
        m_presenter.on_requestVelocityChange(note, event->buttonDownScenePos(Qt::LeftButton).y() - event->scenePos().y());
        m_presenter.on_velocityChangeFinished();
        break;
      case None:
        break;
    }
  }
  event->accept();
  m_action = None;
}
}
