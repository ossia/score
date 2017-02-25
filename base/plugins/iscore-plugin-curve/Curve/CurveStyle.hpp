#pragma once
#include <QColor>
#include <QPen>
#include <iscore_plugin_curve_export.h>

namespace iscore
{
class Skin;
}
namespace Curve
{
struct ISCORE_PLUGIN_CURVE_EXPORT Style
{
  // TODO removeme when msvc starts knowing C++
  Style(const QBrush& c1, const QBrush& c2, const QBrush& c3, const QBrush& c4, const QBrush& c5):
    Point{c1},
    PointSelected{c2},
    Segment{c3},
    SegmentSelected{c4},
    SegmentDisabled{c5}
  {

  }

  const QBrush& Point;         //{128, 215, 62}; // Tender3
  const QBrush& PointSelected; //{233, 208, 89}; // Emphasis2

  const QBrush& Segment;         //{199, 31, 44}; // Tender1
  const QBrush& SegmentSelected; //{216, 178, 24}; // Tender2
  const QBrush& SegmentDisabled; //{127, 127, 127}; // Gray

  QPen PenSegment{};
  QPen PenSegmentSelected{};
  QPen PenSegmentTween{};
  QPen PenSegmentTweenSelected{};
  QPen PenSegmentDisabled{};

  QPen PenPoint{};
  QPen PenPointSelected{};
  QBrush BrushPoint{};
  QBrush BrushPointSelected{};

  void init(const iscore::Skin& s);
  void update();
};

struct ISCORE_PLUGIN_CURVE_EXPORT StyleInterface
{
  virtual ~StyleInterface();
  virtual const Curve::Style& style() const = 0;
};


}
