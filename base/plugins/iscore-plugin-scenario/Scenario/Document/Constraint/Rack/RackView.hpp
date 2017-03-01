#pragma once

#include <QQuickPaintedItem>
#include <QRect>

class QPainter;

class QWidget;

namespace Scenario
{
class RackView final : public QQuickPaintedItem
{
  Q_OBJECT

public:
  RackView(QQuickPaintedItem* parent);
  virtual ~RackView() = default;

  void paint(
      QPainter* painter) override;
};
}
