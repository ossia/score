#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>
namespace Curve
{
class SegmentList final : public score::InterfaceList<SegmentFactory>
{
public:
  using object_type = Curve::SegmentModel;
  virtual ~SegmentList();

  object_type* loadMissing(const VisitorVariant& vis, QObject* parent) const;
};
}
