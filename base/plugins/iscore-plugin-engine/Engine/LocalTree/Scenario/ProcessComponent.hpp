#pragma once
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Scenario/Document/Components/ProcessComponent.hpp>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>
#include <iscore/plugins/customfactory/ModelFactory.hpp>

// TODO clean me up
namespace Engine
{
namespace LocalTree
{
class ISCORE_PLUGIN_ENGINE_EXPORT ProcessComponent
    : public Component<Scenario::GenericProcessComponent<DocumentPlugin>>
{
  ABSTRACT_COMPONENT_METADATA(
      Engine::LocalTree::ProcessComponent,
      "0732ab51-a052-4e2e-a1f7-9bf2926c199c")
public:
  ProcessComponent(
      ossia::net::node_base& node,
      Process::ProcessModel& proc,
      DocumentPlugin& doc,
      const Id<iscore::Component>& id,
      const QString& name,
      QObject* parent);

  virtual ~ProcessComponent();
};

template <typename Process_T>
using ProcessComponent_T
    = Scenario::GenericProcessComponent_T<ProcessComponent, Process_T>;

class ISCORE_PLUGIN_ENGINE_EXPORT ProcessComponentFactory
    : public iscore::
          GenericComponentFactory<Process::ProcessModel, LocalTree::DocumentPlugin, LocalTree::ProcessComponentFactory>
{
  ISCORE_ABSTRACT_COMPONENT_FACTORY(Engine::LocalTree::ProcessComponent)
public:
  virtual ~ProcessComponentFactory();
  virtual ProcessComponent* make(
      const Id<iscore::Component>&,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      LocalTree::DocumentPlugin& doc,
      QObject* paren_objt) const = 0;
};

template <typename ProcessComponent_T>
class ProcessComponentFactory_T
    : public iscore::
          GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
public:
  using model_type = typename ProcessComponent_T::model_type;
  ProcessComponent* make(
      const Id<iscore::Component>& id,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      DocumentPlugin& doc,
      QObject* paren_objt) const override
  {
    return new ProcessComponent_T{id, parent, static_cast<model_type&>(proc),
                                  doc, paren_objt};
  }
};

using ProcessComponentFactoryList = iscore::
    GenericComponentFactoryList<Process::ProcessModel, LocalTree::DocumentPlugin, LocalTree::ProcessComponentFactory>;
}
}
