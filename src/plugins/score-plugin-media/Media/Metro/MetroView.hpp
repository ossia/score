#pragma once
#include <Media/Metro/MetroModel.hpp>
#include <Process/LayerView.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

#include <verdigris>
namespace Media::Metro
{

class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(QGraphicsItem* parent) : Process::LayerView{parent}
  {
    setFlag(QGraphicsItem::ItemClipsToShape);
  }

private:
  void paint_impl(QPainter* p) const override
  {
    // TODO paint the current time
  }
};
}

W_OBJECT_IMPL(Media::Metro::View)
