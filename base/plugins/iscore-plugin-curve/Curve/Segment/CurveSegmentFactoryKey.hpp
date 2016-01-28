#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <QMetaType>
namespace Curve
{
class SegmentModel;
using SegmentFactoryKey = UuidKey<Curve::SegmentModel>;
}

Q_DECLARE_METATYPE(Curve::SegmentFactoryKey)
