#pragma once
#include <score_lib_base_export.h>
#include <QGraphicsItem>
#include <vector>
#include <QPainterPath>

namespace score
{
class ArrowDialog : public QGraphicsItem
{
public:
  ArrowDialog(QGraphicsItem* parent);

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
  const QPainterPath& getPath(QSizeF sz);

  QPainterPath createPath(QSizeF sz);
  static inline std::vector<std::pair<QSizeF, QPainterPath>> paths;
};
}
