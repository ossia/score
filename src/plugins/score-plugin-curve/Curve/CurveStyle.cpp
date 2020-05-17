// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Curve/CurveStyle.hpp>

#include <score/model/Skin.hpp>
namespace Curve
{

StyleInterface::~StyleInterface() { }

void Style::init(const score::Skin& s)
{
  QObject::connect(&s, &score::Skin::changed, [=] { this->update(); });
  update();
}

void Style::update()
{
  PenSegment = QPen{Segment, 2, Qt::PenStyle::SolidLine};
  PenSegmentSelected = QPen{SegmentSelected, 2, Qt::PenStyle::SolidLine};
  PenSegmentTween = QPen{Segment, 2, Qt::PenStyle::DashLine};
  PenSegmentTweenSelected = QPen{SegmentSelected, 2, Qt::PenStyle::DashLine};
  PenSegmentDisabled = QPen{SegmentDisabled, 1, Qt::PenStyle::SolidLine};

  PenPoint = Point.color();
  PenPoint.setCosmetic(true);
  PenPointSelected = PointSelected.color();
  BrushPoint = Point;
  BrushPointSelected = PointSelected;
}
}
