#pragma once

#include <Process/LayerView.hpp>
#include <QString>
#include <QTextLayout>

class QGraphicsItem;
class QPainter;
class QMimeData;

namespace Metronome
{
class LayerView final : public Process::LayerView
{
  Q_OBJECT
public:
  explicit LayerView(QGraphicsItem* parent);
  virtual ~LayerView();

private:
  void paint_impl(QPainter* painter) const override;
  void dropEvent(QGraphicsSceneDragDropEvent* event) override;

};
}
