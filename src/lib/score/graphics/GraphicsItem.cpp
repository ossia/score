// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GraphicsItem.hpp"

#include <score/graphics/ItemBounder.hpp>
#include <score/widgets/ItemViewDrag.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/tools/Debug.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QUrl>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QInputMethod>

using item_help = ossia::hash_map<int, std::pair<QString, QUrl>>;
#if __has_include(<QApplicationStatic>)
#include <QApplicationStatic>
Q_APPLICATION_STATIC(item_help, g_itemHelpRegistry);
#else
Q_GLOBAL_STATIC(item_help, g_itemHelpRegistry);
#endif

void registerItemHelp(int itemType, QString tooltip, QUrl url) noexcept
{
  auto& val = *g_itemHelpRegistry;
  val[itemType] = std::pair<QString, QUrl>{tooltip, url};
}

QUrl getItemHelpUrl(int itemType) noexcept
{
  auto& val = *g_itemHelpRegistry;
  return val[itemType].second;
}


namespace score
{
#if !defined(__EMSCRIPTEN__)
// The WebAssembly implementations live in WasmInputMethod.cpp.
void retargetInputMethod(QObject* target) noexcept
{
  (void)target;
}

void watchSceneInputMethod(QGraphicsScene& scene)
{
  (void)scene;
}
#endif
}

namespace
{
// Destroying an item while the scene is still delivering an event to it
// corrupts the scene's grabber stack. QGraphicsScenePrivate::ungrabMouse sends
// QEvent::UngrabMouse and only *afterwards* does mouseGrabberItems.takeLast():
// an item that removes itself from the scene in that handler is popped by
// removeItemHelper first, so the pending takeLast() then takes somebody else's
// grab -- or runs on an empty list. Only the current grabber is exposed to
// this, which is what makes it cheap to detect.
bool isDeliveringToItself(QGraphicsItem* item) noexcept
{
  auto* sc = item->scene();
  return sc && sc->mouseGrabberItem() == item;
}

// Context for the deferred deletion, so that it is cancelled rather than left
// with a dangling pointer if the item goes away by some other route first.
// toGraphicsObject() is not enough: score's graphics widgets inherit QObject
// and QGraphicsItem separately rather than deriving from QGraphicsObject.
QObject* deferralContext(QGraphicsItem* item) noexcept
{
  if(auto* obj = dynamic_cast<QObject*>(item))
    return obj;
  return item->scene();
}
}

void deleteGraphicsObject(QGraphicsObject* item)
{
  if(item)
  {
    if(isDeliveringToItself(item))
    {
      QMetaObject::invokeMethod(
          item, [item] { deleteGraphicsObject(item); }, Qt::QueuedConnection);
      return;
    }

    auto sc = item->scene();

    if(sc)
    {
      sc->removeItem(item);
    }

    item->deleteLater();
  }
}

void deleteGraphicsItem(QGraphicsItem* item)
{
  if(item)
  {
    if(isDeliveringToItself(item))
    {
      QMetaObject::invokeMethod(
          deferralContext(item), [item] { deleteGraphicsItem(item); },
          Qt::QueuedConnection);
      return;
    }

    auto sc = item->scene();

    if(sc)
    {
      sc->removeItem(item);
    }

    delete item;
  }
}

QGraphicsView* getView(const QGraphicsItem& self)
{
  if(!self.scene())
    return nullptr;
  auto v = self.scene()->views();
  if(v.empty())
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
  double ratio = qGuiApp->devicePixelRatio();
  QImage img(
      std::ceil(logical_w * ratio), logical_h * ratio,
      QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(ratio);
  img.fill(Qt::transparent);
  return img;
}

std::optional<QPointF> mapPointToItem(QPoint global, QGraphicsItem& item)
{
  // Get the QGraphicsView
  auto views = item.scene()->views();
  if(views.empty())
    return std::nullopt;

  auto view = views.front();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(global);
  auto scene_pt = view->mapToScene(view_pt);
  return item.mapFromScene(scene_pt);
}

namespace score
{
std::pair<double, bool>
ItemBounder::bound(QGraphicsItem* parent, double x0, double w) noexcept
{
  auto view = getView(*parent);
  int item_left = view->mapFromScene(parent->mapToScene({x0, 0.})).x();
  int item_right = item_left + w;

  double x = x0;
  const double min_x = x0;
  const double max_x = view->width() - 30.;

  if(item_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - item_left + min_x;
  }
  else if(item_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - item_right + max_x;
  }
  x = std::max(x, 2 * x0);

  if(std::abs(m_x - x) > 0.1)
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
