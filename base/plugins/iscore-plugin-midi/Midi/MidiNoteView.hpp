#pragma once
#include <Midi/MidiNote.hpp>
#include <QQuickPaintedItem>
#include <QObject>

namespace Midi
{
class NoteView final : public QQuickPaintedItem
{
  Q_OBJECT
public:
  const Note& note;

  NoteView(const Note& n, QQuickPaintedItem* parent);

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
      QPainter* painter) override;

signals:
  void noteChanged(int, double); // pitch, scaled between [0; 1]
  void noteChangeFinished();
  void noteScaled(double);

private:
  QVariant
  itemChange(GraphicsItemChange change, const QVariant& value) override;
  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  double m_width{};
  double m_height{};

  bool m_scaling = false;
};
}
