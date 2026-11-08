#pragma once
#include <QGraphicsItem>
#include <QStyleOptionGraphicsItem>

#include <score_lib_base_export.h>

namespace score
{
//! Control widget size, relative to its default
enum class ControlSize
{
  Normal,
  Small,
  Large
};

//! data() key: show the value only while hovered or dragged
inline constexpr int ValueOnHoverDataKey = 0x5C0E;

//! Whether to draw the value text. Called on every paint, must stay cheap.
inline bool showsValue(
    const QGraphicsItem& item, bool grabbed,
    const QStyleOptionGraphicsItem* option) noexcept
{
  if(grabbed || !item.acceptHoverEvents() || !item.data(ValueOnHoverDataKey).toBool())
    return true;
  return option && (option->state & QStyle::State_MouseOver);
}

//! data() key: where a knob draws its value under it, as a Qt::Alignment
inline constexpr int ValueAlignmentDataKey = 0x5C0F;

//! Where a knob draws its value under it: Qt::AlignLeft puts it at the knob's
//! left edge. No-op for widgets other than knobs.
SCORE_LIB_BASE_EXPORT void setValueAlignment(QGraphicsItem& control, Qt::Alignment align);

//! No-op for widgets other than knobs and sliders
SCORE_LIB_BASE_EXPORT void setValueOnHover(QGraphicsItem& control, bool onHover);

//! No-op for widgets other than knobs and sliders
SCORE_LIB_BASE_EXPORT void setControlSize(QGraphicsItem& control, ControlSize size);
}
