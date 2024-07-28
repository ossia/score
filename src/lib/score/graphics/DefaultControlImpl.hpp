#pragma once
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPointer>
#include <QTimer>

namespace score
{
struct DefaultControlImpl
{
  static void editWidgetInContextMenu(
      auto& self, QGraphicsScene* scene, QGraphicsItem* obj, double v)
  {
    self.m_value = self.unmap(v);

    QPointer p{&self};
    if constexpr(requires { self.m_noValueChangeOnMove; })
    {
      if(!self.m_noValueChangeOnMove)
        self.sliderMoved();
    }
    else
    {
      self.sliderMoved();
    }

    if(p)
    {
      self.update();
    }
    else
    {
      QTimer::singleShot(0, &self, [scene, obj] {
        scene->removeItem(obj);
        delete obj;
      });
    }
  }
};
}
