#pragma once
#include <QColor>
#include <QPen>

#include <score_plugin_curve_export.h>

namespace score
{
class Skin;
}
namespace Curve
{
struct SCORE_PLUGIN_CURVE_EXPORT Style
{
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

  void init(const score::Skin& s);
  void update();
};

struct SCORE_PLUGIN_CURVE_EXPORT StyleInterface
{
  virtual ~StyleInterface();
  virtual const Curve::Style& style() const = 0;
};
}
