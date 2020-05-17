#pragma once
#include <Mapping/MappingModel.hpp>

#include <LocalTree/ProcessComponent.hpp>

namespace LocalTree
{
class MappingComponent final : public ProcessComponent_T<Mapping::ProcessModel>
{
  COMPONENT_METADATA("30bec9cc-c495-4a6d-bd91-ec1313cc9078")

public:
  MappingComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Mapping::ProcessModel& proc,
      const score::DocumentContext& ctx,
      QObject* parent_obj)
      : ProcessComponent_T<Mapping::ProcessModel>{
          parent,
          proc,
          ctx,
          id,
          "MappingComponent",
          parent_obj}
  {
    add<Mapping::ProcessModel::p_sourceMin>(proc);
    add<Mapping::ProcessModel::p_sourceMax>(proc);
    add<Mapping::ProcessModel::p_targetMin>(proc);
    add<Mapping::ProcessModel::p_targetMax>(proc);
  }
};

using MappingComponentFactory = ProcessComponentFactory_T<MappingComponent>;
}
