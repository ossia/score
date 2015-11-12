#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

using ProtocolList = GenericFactoryList_T<ProtocolFactory, ProtocolFactoryKey>;
