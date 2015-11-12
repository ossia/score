#pragma once
#include <Curve/Segment/CurveSegmentFactory.hpp>


using CurveSegmentList = GenericFactoryList_T<CurveSegmentFactory, CurveSegmentFactoryKey>;

class SingletonCurveSegmentList
{
    public:
        SingletonCurveSegmentList() = delete;
        static CurveSegmentList& instance();
};
