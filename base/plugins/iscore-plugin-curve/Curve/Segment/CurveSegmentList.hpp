#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

class DynamicCurveSegmentList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(CurveSegmentFactory)
};
