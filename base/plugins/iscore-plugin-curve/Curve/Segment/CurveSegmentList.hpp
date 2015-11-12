#pragma once
#include "CurveSegmentFactory.hpp"

// TODO Template this
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

using CurveSegmentList = GenericFactoryList_T<CurveSegmentFactory>;

class SingletonCurveSegmentList
{
    public:
        SingletonCurveSegmentList() = delete;
        static CurveSegmentList& instance();
};
