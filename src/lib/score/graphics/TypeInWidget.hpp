#pragma once
#include <score/graphics/RightClickWidget.hpp>

#include <QPointF>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <vector>

class QGraphicsScene;
class QGraphicsProxyWidget;

namespace score
{
//! One field of a type-in box: "x", "min", "hue"...
struct TypeInField
{
  QString prefix;
  double min{};
  double max{};
  double value{};
  int decimals{6};
};

/**
 * @brief The type-in box a right-click raises over a control with more than
 * one number in it.
 *
 * A single spin box can be closed on QAbstractSpinBox::editingFinished, which
 * is what DefaultGraphicsSliderImpl does. Several of them cannot: that signal
 * is also emitted when the focus merely leaves a box, and moving from x to y
 * is such a focus-out. Closing on it made every field but the first
 * unreachable -- clicking y took the whole box down.
 *
 * The box goes away when the focus leaves it as a whole, which is the proxy
 * item losing the scene focus. QApplication::focusWidget() cannot answer that
 * question: a widget embedded in a QGraphicsProxyWidget never becomes the
 * application's focus widget, the view stays it whatever is typed into.
 *
 * @param onChanged  (field index, value), as a field is typed into.
 * @param onFinished once per field actually edited, when that edit is done:
 *                   the release that turns the ongoing command into one undo
 *                   step.
 *
 * The box registers itself as the one currentRightClickWidget(), so raising
 * any other takes it down.
 */
SCORE_LIB_BASE_EXPORT
QGraphicsProxyWidget* showTypeInBox(
    QGraphicsScene& scene, QPointF scenePos, const std::vector<TypeInField>& fields,
    std::function<void(int, double)> onChanged, std::function<void()> onFinished);
}
