#pragma once
#include <QQuickPaintedItem>
#include <QRect>
#include <QSize>

namespace Process
{
class LayerModel;
}
class ProcessPanelPresenter;
class QPainter;

class QWidget;

class ProcessPanelGraphicsProxy final : public QQuickPaintedItem
{
  QSizeF m_size;

public:
  ProcessPanelGraphicsProxy();

  QRectF boundingRect() const override;
  void paint(
      QPainter* painter) override;

  void setRect(const QSizeF& size);
};
