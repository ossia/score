// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSegmentFactory.hpp"

#include "CurveSegmentList.hpp"
namespace Curve
{
SegmentFactory::~SegmentFactory() { }

SegmentList::~SegmentList() { }

SegmentList::object_type*
SegmentList::loadMissing(const VisitorVariant& vis, QObject* parent) const
{
  SCORE_TODO;
  return nullptr;
}
}
