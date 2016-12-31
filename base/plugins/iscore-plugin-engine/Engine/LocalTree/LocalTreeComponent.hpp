#pragma once
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/LocalTree/NameProperty.hpp>
#include <Engine/LocalTree/Scenario/MetadataParameters.hpp>
#include <iscore/model/Component.hpp>
namespace Engine
{
namespace LocalTree
{
template <typename Component_T>
class Component : public Component_T
{
public:
  template <typename... Args>
  Component(ossia::net::node_base& n, iscore::ModelMetadata& m, Args&&... args)
      : Component_T{std::forward<Args>(args)...}, m_thisNode{n, m, this}
  {
    make_metadata_node(m, m_thisNode.node, m_properties, this);
  }

  ossia::net::node_base& node() const
  {
    return m_thisNode.node;
  }

  auto& context() const { return this->system().context(); }

protected:
  MetadataNamePropertyWrapper m_thisNode;
  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using CommonComponent = Component<iscore::GenericComponent<DocumentPlugin>>;
}
}
