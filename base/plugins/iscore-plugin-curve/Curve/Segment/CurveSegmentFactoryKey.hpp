#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <QMetaType>

class CurveSegmentTag{};
using CurveSegmentFactoryKey = StringKey<CurveSegmentTag>;
Q_DECLARE_METATYPE(CurveSegmentFactoryKey)
