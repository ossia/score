#include "ItemViewDrag.hpp"

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QCursor>
#include <QDrag>
#include <QPainter>
#include <QPixmap>
#include <QStyleOptionViewItem>
#include <QWidget>

namespace score
{
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
