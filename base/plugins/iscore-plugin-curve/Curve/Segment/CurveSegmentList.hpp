#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

using CurveSegmentList = GenericFactoryMap_T<CurveSegmentFactory, CurveSegmentFactoryKey>;

class DynamicCurveSegmentList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(CurveSegmentFactory)
};
