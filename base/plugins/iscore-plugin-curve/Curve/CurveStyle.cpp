#include <Curve/CurveStyle.hpp>

#include <iscore/model/Skin.hpp>
namespace Curve
{


StyleInterface::~StyleInterface()
{

}

void Style::init(const iscore::Skin& s)
{
  QObject::connect(&s, &iscore::Skin::changed, [=] { this->update(); });
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
