#pragma once
#include <Midi/MidiNote.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <verdigris>

namespace Midi
{
class View;
class NoteView final : public QObject, public QGraphicsItem
{
  W_OBJECT(NoteView)
  Q_INTERFACES(QGraphicsItem)
public:
  const Note& note;

  NoteView(const Note& n, View* parent);

  void setWidth(qreal w) noexcept
  {
    if (m_width != w)
    {
      prepareGeometryChange();
      m_width = w;
    }
  }

  void setHeight(qreal h) noexcept
  {
    if (m_height != h)
    {
      prepareGeometryChange();
      m_height = h;
    }
  }

  QRectF boundingRect() const override { return {0, 0, m_width, m_height}; }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  QRectF computeRect() const noexcept;
  QPointF closestPos(QPointF note) const noexcept;

  // TODO we should frankly not emit signals and just call the presenter
  // directly... 5 signals * 1000 notes = a lot of wasted memory
  void noteChanged(int arg_1, double arg_2)
      W_SIGNAL(noteChanged, arg_1, arg_2); // pitch, scaled between [0; 1]
  void noteChangeFinished() W_SIGNAL(noteChangeFinished);
  void noteScaled(double arg_1) W_SIGNAL(noteScaled, arg_1);
  void deselectOtherNotes() W_SIGNAL(deselectOtherNotes)
  void requestVelocityChange(double v) W_SIGNAL(requestVelocityChange, v)
  void velocityChangeFinished() W_SIGNAL(velocityChangeFinished)

private:
  bool canEdit() const;
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  qreal m_width{};
  qreal m_height{};

  bool m_scaling = false;
  bool m_velocityChange = false;
};
}
