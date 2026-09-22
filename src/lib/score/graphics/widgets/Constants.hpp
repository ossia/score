#pragma once
#include <QGraphicsItem>
#include <QRectF>

#include <score_lib_base_export.h>

#include <algorithm>
#include <array>
#include <cstddef>
namespace score
{
static const constexpr QRectF defaultSliderSize{0., 0., 60., 23.};
static const constexpr QRectF defaultKnobSize{0., 0., 35., 35.};
static const constexpr QRectF defaultCheckBoxSize{0., 0., 20., 20.};
static const constexpr QRectF defaultToggleSize{0., 0., 60., 20.};
static const constexpr QRectF defaultRangeSliderSize{0., 0., 60., 23.};

struct DoubleSpinboxWithEnter;
struct DefaultControlImpl;
struct DefaultGraphicsSliderImpl;
struct DefaultGraphicsSpinboxImpl;
struct DefaultGraphicsKnobImpl;

//! Vector controls: while this is held, dragging one component drags the
//! others along with it.
static const constexpr auto LinkComponentsModifier = Qt::ShiftModifier;

SCORE_LIB_BASE_EXPORT bool linkComponentsRequested() noexcept;

//! Give every component but `source` the delta that `source` just took.
//! Normalized units: each one moves by the same fraction of its own range and
//! stops at its own bounds.
template <typename T, std::size_t N, typename Set>
void linkComponents(
    const std::array<T, N>& previous, std::size_t source, T current, Set&& set) noexcept
{
  const T delta = current - previous[source];
  for(std::size_t i = 0; i < N; i++)
    if(i != source)
      set(i, std::clamp(T(previous[i] + delta), T(0), T(1)));
}

static constexpr auto GraphicsItemType = QGraphicsItem::UserType + 90000;
#define SCORE_GRAPHICS_ITEM_TYPE(value) \
private:                                \
  Q_INTERFACES(QGraphicsItem)           \
public:                                 \
  enum                                  \
  {                                     \
    Type = GraphicsItemType + value     \
  };                                    \
  int type() const override             \
  {                                     \
    return Type;                        \
  }                                     \
                                        \
private:
}
