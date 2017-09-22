#pragma once

#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;

namespace Mapping
{
class LayerView final : public Process::LayerView
{
    Q_OBJECT
public:
  explicit LayerView(QGraphicsItem* parent);

private:
  void paint_impl(QPainter* painter) const override;
};
}
