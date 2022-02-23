// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GraphicsItem.hpp"

#include <score/graphics/ItemBounder.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/tools/Debug.hpp>

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGuiApplication>

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
  QImage img(
      logical_w * ratio,
      logical_h * ratio,
      QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(ratio);
  img.fill(Qt::transparent);
  return img;
}


std::optional<QPointF> mapPointToItem(QPoint global, QGraphicsItem& item)
{
  // Get the QGraphicsView
  auto views = item.scene()->views();
  if (views.empty())
    return std::nullopt;

  auto view = views.front();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(global);
  auto scene_pt = view->mapToScene(view_pt);
  return item.mapFromScene(scene_pt);
}


namespace score
{
std::pair<double, bool> ItemBounder::bound(QGraphicsItem* parent, double x0, double w) noexcept
{
  auto view = getView(*parent);
  int item_left = view->mapFromScene(parent->mapToScene({x0, 0.})).x();
  int item_right = item_left + w;

  double x = x0;
  const double min_x = x0;
  const double max_x = view->width() - 30.;

  if (item_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - item_left + min_x;
  }
  else if (item_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - item_right + max_x;
  }
  x = std::max(x, 2 * x0);

  if (std::abs(m_x - x) > 0.1)
  {
    m_x = x;
    return {x, true};
  }
  else
  {
    return {x, false};
  }
}
}
