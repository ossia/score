#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>


using CurveSegmentList = GenericFactoryMap_T<CurveSegmentFactory, CurveSegmentFactoryKey>;

class SingletonCurveSegmentList
{
    public:
        SingletonCurveSegmentList() = delete;
        static CurveSegmentList& instance();
};
