#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>

#include <score/plugins/InterfaceList.hpp>

#include <score_plugin_curve_export.h>

namespace Curve
{
class SCORE_PLUGIN_CURVE_EXPORT SegmentList final
    : public score::InterfaceList<SegmentFactory>
{
public:
  using object_type = Curve::SegmentModel;
  virtual ~SegmentList();

  object_type* loadMissing(const VisitorVariant& vis, QObject* parent) const;
};
}
