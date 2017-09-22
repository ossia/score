#pragma once
#include <Process/LayerView.hpp>

namespace Skeleton
{
class View final : public Process::LayerView
{
    Q_OBJECT
  public:
    explicit View(QGraphicsItem* parent);
    ~View();

  private:
    void paint_impl(QPainter*) const override;
};
}
