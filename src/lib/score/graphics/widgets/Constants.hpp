#pragma once
#include <QGraphicsItem>
#include <QRectF>

#include <score_lib_base_export.h>
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
