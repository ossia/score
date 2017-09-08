#pragma once
#include <QGraphicsObject>
#include <QImage>

class QGraphicsButton final : public QGraphicsObject
{
    Q_OBJECT
  public:
    QGraphicsButton(QImage clicked, QImage released);

  signals:
    void clicked();

  private:
    QRectF boundingRect() const override;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    QImage m_clicked, m_released;
    bool m_click{false};
};
