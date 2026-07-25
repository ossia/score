#include "ItemViewDrag.hpp"

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QCursor>
#include <QDrag>
#include <QPainter>
#include <QPixmap>
#include <QStyleOptionViewItem>
#include <QWidget>

#if defined(__EMSCRIPTEN__)
#include <emscripten/em_asm.h>
#endif

namespace score
{
void installDragImageWorkaround()
{
#if defined(__EMSCRIPTEN__)
  static bool installed = false;
  if(installed)
    return;
  installed = true;

  // To start an HTML5 drag Qt hands DataTransfer::setDragImage() a real DOM
  // element, which the browser only snapshots if it is attached and rendered.
  // QWasmDrag therefore inserts one as the first child of the window container
  // and "hides" it by stacking alone: qwasmcssstyle.cpp gives .hidden-drag-image
  //   visibility: visible; display: block; opacity: 1.0;
  //   position: absolute; z-index: -1;
  // with no top/left, so it sits at the container's origin and is only hidden
  // as long as something opaque paints above it. When that stacking assumption
  // does not hold -- which it does not here -- the drag image is painted in the
  // top-left corner of the page for the duration of every drag.
  //
  // Move it off-screen instead of hiding it: display/visibility/opacity all
  // have to keep their values or the browser refuses to snapshot the element,
  // but an absolutely positioned box at negative coordinates still snapshots
  // correctly. The rule has to go inside the shadow root, since that is where
  // both Qt's stylesheet and the element itself live.
  // clang-format off
  EM_ASM({
    (function install() {
      var host = document.querySelector("#qt-shadow-container");
      var root = host ? host.shadowRoot : null;
      if (!root) {
        // The platform plugin has not built its shadow root yet.
        requestAnimationFrame(install);
        return;
      }
      if (root.getElementById("score-drag-image-offscreen"))
        return;
      var style = document.createElement("style");
      style.id = "score-drag-image-offscreen";
      style.textContent =
        ".hidden-drag-image { left: -99999px !important; top: -99999px !important; }";
      root.appendChild(style);
    })();
  });
  // clang-format on
#endif
}

void setItemViewDragPixmap(
    QDrag& drag, QAbstractItemView& view, const QStyleOptionViewItem& option,
    const QModelIndexList& indexes)
{
  auto* viewport = view.viewport();
  if(!viewport)
    return;

  const QRect viewportRect = viewport->rect();
  std::vector<std::pair<QModelIndex, QRect>> items;
  QRect bounds;
  for(const QModelIndex& index : indexes)
  {
    const QRect r = view.visualRect(index);
    if(!r.intersects(viewportRect))
      continue;
    items.emplace_back(index, r);
    bounds |= r;
  }

  bounds = bounds.intersected(viewportRect);
  if(items.empty() || bounds.isEmpty())
    return;

  const qreal dpr = view.devicePixelRatioF();
  QPixmap pixmap(bounds.size() * dpr);
  pixmap.setDevicePixelRatio(dpr);
  pixmap.fill(Qt::transparent);

  {
    QPainter painter{&pixmap};
    QStyleOptionViewItem opt = option;
    opt.state |= QStyle::State_Selected;
    for(const auto& [index, rect] : items)
    {
      opt.rect = rect.translated(-bounds.topLeft());
      if(auto* delegate = view.itemDelegateForIndex(index))
        delegate->paint(&painter, opt, index);
    }
  }

  drag.setPixmap(pixmap);
  drag.setHotSpot(viewport->mapFromGlobal(QCursor::pos()) - bounds.topLeft());
}
}
