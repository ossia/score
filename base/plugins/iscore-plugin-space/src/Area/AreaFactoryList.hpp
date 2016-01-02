#pragma once
#include <src/Area/AreaFactory.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>

namespace Space
{
using AreaFactoryList = GenericFactoryMap_T<AreaFactory, AreaFactoryKey>;
}
