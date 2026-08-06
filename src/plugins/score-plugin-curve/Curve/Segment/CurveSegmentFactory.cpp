// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSegmentFactory.hpp"

#include "CurveSegmentList.hpp"

#include <QDebug>
namespace Curve
{
SegmentFactory::~SegmentFactory() { }

SegmentList::~SegmentList() { }

SegmentList::object_type* SegmentList::loadMissing(
    const UuidKey<Curve::SegmentFactory>& key, const VisitorVariant& vis,
    QObject* parent) const
{
  // Deliberately not preserved, unlike processes, ports, devices and document
  // plug-ins. A segment is asked for its value at a point -- valueAt,
  // makeDoubleFunction -- and those answers are played. A stand-in would have
  // to invent them, and silently wrong automation is worse than a missing
  // segment. It also cannot arise: segments come only from score-plugin-curve,
  // so a build without it has no curves to put them in.
  qWarning() << "Dropping a curve segment of unknown type"
             << score::uuids::toByteArray(key.impl());
  return nullptr;
}
}
