#pragma once
#include <score/model/Component.hpp>

#include <LocalTree/GetProperty.hpp>
#include <LocalTree/NameProperty.hpp>
#include <LocalTree/Property.hpp>

namespace LocalTree
{
template <typename Component_T>
class Component : public Component_T
{
public:
  template <typename... Args>
  Component(ossia::net::node_base& n, score::ModelMetadata& m, Args&&... args)
      : Component_T{std::forward<Args>(args)...}, m_thisNode{n, m, this}
  {
    add<score::ModelMetadata::p_comment>(m);
    add<score::ModelMetadata::p_label>(m);
  }

  ossia::net::node_base& node() const { return m_thisNode.node; }

  auto& context() const { return this->system(); }

protected:
  template <typename Property, typename Object>
  void add(Object& obj)
  {
    m_properties.push_back(add_property<Property>(node(), obj, this));
  }

  template <typename Property, typename Object>
  void add_get(Object& obj)
  {
    m_properties.push_back(add_getProperty<Property>(node(), obj, this));
  }

  MetadataNamePropertyWrapper m_thisNode;
  std::vector<std::unique_ptr<BaseProperty>> m_properties;
};

using CommonComponent = Component<score::GenericComponent<const score::DocumentContext>>;
}
