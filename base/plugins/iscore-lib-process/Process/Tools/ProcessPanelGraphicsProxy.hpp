#pragma once
#include <QGraphicsItem>
#include <QRect>
#include <QSize>

namespace Process
{
class LayerModel;
}
class ProcessPanelPresenter;
class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

class ProcessPanelGraphicsProxy final : public QGraphicsItem
{
  QSizeF m_size;

public:
  ProcessPanelGraphicsProxy();

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter, const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;

  void setRect(const QSizeF& size);
};
