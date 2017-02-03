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
  const QColor& Point;         //{128, 215, 62}; // Tender3
  const QColor& PointSelected; //{233, 208, 89}; // Emphasis2

  const QColor& Segment;         //{199, 31, 44}; // Tender1
  const QColor& SegmentSelected; //{216, 178, 24}; // Tender2
  const QColor& SegmentDisabled; //{127, 127, 127}; // Gray

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
