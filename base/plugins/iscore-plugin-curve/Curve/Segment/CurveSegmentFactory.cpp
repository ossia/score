#include "CurveSegmentFactory.hpp"
#include "CurveSegmentList.hpp"
namespace Curve
{
SegmentFactory::~SegmentFactory()
{

}

SegmentList::~SegmentList()
{

}

SegmentList::object_type* SegmentList::loadMissing(const VisitorVariant& vis, QObject* parent) const
{
    ISCORE_TODO;
    return nullptr;
}
}
