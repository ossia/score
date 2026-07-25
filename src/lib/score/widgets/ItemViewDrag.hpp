#pragma once
#include <QModelIndexList>

#include <score_lib_base_export.h>

class QAbstractItemView;
class QDrag;
class QStyleOptionViewItem;

namespace score
{
/**
 * @brief Give a QDrag the drag image QAbstractItemView::startDrag would have made.
 *
 * Renders the dragged indices through the view's item delegate, the way the
 * default startDrag implementation does. Views that override startDrag and do
 * not set a pixmap otherwise leave QDrag::pixmap() null, which the platform
 * plugin is then free to fill in with whatever it likes.
 */
SCORE_LIB_BASE_EXPORT
void setItemViewDragPixmap(
    QDrag& drag, QAbstractItemView& view, const QStyleOptionViewItem& option,
    const QModelIndexList& indexes);

/**
 * @brief Keep Qt's temporary drag-image element out of sight.
 *
 * No-op outside of WebAssembly. Idempotent, and safe to call before the
 * platform plugin has built its shadow root.
 */
SCORE_LIB_BASE_EXPORT
void installDragImageWorkaround();
}
