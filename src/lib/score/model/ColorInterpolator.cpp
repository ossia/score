#include "ColorInterpolator.hpp"
#include <QPen>
#include <array>

#include <score/tools/Debug.hpp>
#include <ossia/detail/flat_map.hpp>
#include <ossia/editor/curve/curve_segment/easing.hpp>
#include <ossia/network/dataspace/color.hpp>
#include <ossia/network/dataspace/value_with_unit.hpp>
namespace score
{
namespace
{
struct ColorInterpolator
{
  std::array<QPen, 60> pens;
  ColorInterpolator() noexcept { }

  void init(QColor c1, QColor c2, QPen sourcePen) noexcept
  {
    ossia::hunter_lab col1 = ossia::rgb(c1.redF(), c1.greenF(), c1.blueF());
    ossia::hunter_lab col2 = ossia::rgb(c2.redF(), c2.greenF(), c2.blueF());
    for(int i = 0; i < 60; i++)
    {
      const float t = float(i + 1) / 60.f;
      const float L = ossia::easing::ease{}(col1.dataspace_value[0], col2.dataspace_value[0], t);
      const float a = ossia::easing::ease{}(col1.dataspace_value[1], col2.dataspace_value[1], t);
      const float b = ossia::easing::ease{}(col1.dataspace_value[2], col2.dataspace_value[2], t);

      ossia::rgb rgb{ossia::hunter_lab{L, a, b}};
      pens[i] = sourcePen;
      pens[i].setColor(QColor::fromRgbF(rgb.dataspace_value[0], rgb.dataspace_value[1], rgb.dataspace_value[2]));
    }
  }
};

static inline ossia::flat_map<std::tuple<QRgb, QRgb, double, Qt::PenStyle>, ColorInterpolator> interpolators;
static const ColorInterpolator& getInterpolator(QColor c1, QColor c2, const QPen& sp) noexcept
{
  auto k = std::make_tuple(c1.rgb(), c2.rgb(), sp.widthF(), sp.style());
  if(auto it = interpolators.find(k); it != interpolators.end())
    return it->second;

  auto& interp = interpolators[k];
  interp.init(c1, c2, sp);
  return interp;
}
}

const QPen& score::ColorBang::getNextPen(QColor c1, QColor c2, const QPen& pen) noexcept
{
  SCORE_ASSERT(pos > 0 && pos <= 59);
  return getInterpolator(c1, c2, pen).pens[pos--];
}

void score::ColorBang::start() noexcept
{
  pos = 59;
}

void score::ColorBang::stop() noexcept
{
  pos = 0;
}

bool ColorBang::running() const noexcept
{
  return pos > 0;
}
}
