#pragma once
#include <Midi/MidiNote.hpp>
#include <QGraphicsItem>
#include <QObject>

namespace Midi
{
class NoteView final : public QObject, public QGraphicsItem
{
  Q_OBJECT
  Q_INTERFACES(QGraphicsItem)
public:
  const Note& note;

  NoteView(const Note& n, QGraphicsItem* parent);

  void setWidth(double w)
  {
    prepareGeometryChange();
    m_width = w;
  }

  void setHeight(double h)
  {
    prepareGeometryChange();
    m_height = h;
  }

  QRectF boundingRect() const override
  {
    return {0, 0, m_width, m_height};
  }

  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

Q_SIGNALS:
  void noteChanged(int, double); // pitch, scaled between [0; 1]
  void noteChangeFinished();
  void noteScaled(double);

private:
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

  double m_width{};
  double m_height{};

  bool m_scaling = false;
};
}
