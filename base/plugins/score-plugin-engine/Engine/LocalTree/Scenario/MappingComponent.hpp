#pragma once
#include <Engine/LocalTree/Scenario/ProcessComponent.hpp>
#include <Mapping/MappingModel.hpp>

namespace Engine
{
namespace LocalTree
{
class MappingComponent : public ProcessComponent_T<Mapping::ProcessModel>
{
  COMPONENT_METADATA("30bec9cc-c495-4a6d-bd91-ec1313cc9078")

public:
  MappingComponent(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Mapping::ProcessModel& proc,
      DocumentPlugin& ctx,
      QObject* parent_obj)
      : ProcessComponent_T<Mapping::ProcessModel>{
          parent, proc, ctx, id, "MappingComponent", parent_obj}
  {
    m_properties.push_back(add_property<double>(
        node(), "sourceMin", &proc,
        &Mapping::ProcessModel::sourceMin,
        &Mapping::ProcessModel::setSourceMin,
        &Mapping::ProcessModel::sourceMinChanged, this));
    m_properties.push_back(add_property<double>(
        node(), "sourceMax", &proc,
        &Mapping::ProcessModel::sourceMax,
        &Mapping::ProcessModel::setSourceMax,
        &Mapping::ProcessModel::sourceMaxChanged, this));
    m_properties.push_back(add_property<double>(
        node(), "targetMin", &proc,
        &Mapping::ProcessModel::targetMin,
        &Mapping::ProcessModel::setTargetMin,
        &Mapping::ProcessModel::targetMinChanged, this));
    m_properties.push_back(add_property<double>(
        node(), "targetMax", &proc,
        &Mapping::ProcessModel::targetMax,
        &Mapping::ProcessModel::setTargetMax,
        &Mapping::ProcessModel::targetMaxChanged, this));
  }

private:
  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using MappingComponentFactory = ProcessComponentFactory_T<MappingComponent>;
}
}
