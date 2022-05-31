#pragma once
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
struct DefaultGraphicsSliderImpl;
}
