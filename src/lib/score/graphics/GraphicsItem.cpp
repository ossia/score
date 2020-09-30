// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GraphicsItem.hpp"

#include <score/plugins/UuidKey.hpp>
#include <score/tools/Debug.hpp>

#include <QGuiApplication>
#include <QGraphicsItem>
#include <QGraphicsView>

void deleteGraphicsObject(QGraphicsObject* item)
{
  if (item)
  {
    auto sc = item->scene();

    if (sc)
    {
      sc->removeItem(item);
    }

    item->deleteLater();
  }
}

void deleteGraphicsItem(QGraphicsItem* item)
{
  if (item)
  {
    auto sc = item->scene();

    if (sc)
    {
      sc->removeItem(item);
    }

    delete item;
  }
}

QGraphicsView* getView(const QGraphicsItem& self)
{
  if (!self.scene())
    return nullptr;
  auto v = self.scene()->views();
  if (v.empty())
    return nullptr;
  return v.first();
}

// TODO apparently crashes on macOS... investigate
QGraphicsView* getView(const QPainter& painter)
{
  auto widg = static_cast<QWidget*>(painter.device());
  SCORE_ASSERT(widg);
  return static_cast<QGraphicsView*>(widg->parent());
}

QImage newImage(double logical_w, double logical_h)
{
  double ratio = qApp->devicePixelRatio();
  QImage img(logical_w * ratio, logical_h * ratio, QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(ratio);
  img.fill(Qt::transparent);
  return img;
}
