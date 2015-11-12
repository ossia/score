#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>

using ProtocolList = GenericFactoryMap_T<ProtocolFactory, ProtocolFactoryKey>;
