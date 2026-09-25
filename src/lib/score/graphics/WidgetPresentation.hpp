#pragma once
#include <QGraphicsItem>
#include <QStyleOptionGraphicsItem>

#include <score_lib_base_export.h>

namespace score
{
//! How large a control widget is drawn, relative to its default size.
enum class ControlSize
{
  Normal,
  Small, //!< for secondary controls, in dense rows
  Large  //!< for the one or two controls a section is about
};

//! QGraphicsItem::data() key under which a control widget records that it
//! shows its value only while hovered or dragged.
inline constexpr int ValueOnHoverDataKey = 0x5C0E;

//! Whether a control widget should draw its value text now. Called on every
//! paint: widgets that take no hover events (all but the hover-only ones)
//! return at once, and hovering is read from the paint's own style option.
inline bool showsValue(
    const QGraphicsItem& item, bool grabbed,
    const QStyleOptionGraphicsItem* option) noexcept
{
  if(grabbed || !item.acceptHoverEvents() || !item.data(ValueOnHoverDataKey).toBool())
    return true;
  return option && (option->state & QStyle::State_MouseOver);
}

//! Knobs and sliders: draw the value only while hovered or dragged, keeping
//! dense rows readable. Other widgets are left as they are.
SCORE_LIB_BASE_EXPORT void setValueOnHover(QGraphicsItem& control, bool onHover);

//! Knobs and sliders: resize from their default size. Other widgets are left
//! as they are.
SCORE_LIB_BASE_EXPORT void setControlSize(QGraphicsItem& control, ControlSize size);
}
