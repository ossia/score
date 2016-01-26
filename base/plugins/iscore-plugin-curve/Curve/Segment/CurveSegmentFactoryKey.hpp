#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <QMetaType>
namespace Curve
{
class SegmentTag{};
using SegmentFactoryKey = UuidKey<SegmentTag>;
}

Q_DECLARE_METATYPE(Curve::SegmentFactoryKey)
