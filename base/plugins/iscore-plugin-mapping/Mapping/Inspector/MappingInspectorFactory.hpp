#pragma once
#include <Mapping/Inspector/MappingInspectorWidget.hpp>
#include <Mapping/MappingModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

namespace Mapping
{
class MappingInspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  ISCORE_CONCRETE_FACTORY("14b3dc85-6152-4526-8d61-6b038ec5d676")
};
}
