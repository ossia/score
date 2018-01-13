#pragma once
#include <Engine/LocalTree/LocalTreeComponent.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Process/ProcessComponent.hpp>
#include <score/model/Component.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score/plugins/customfactory/ModelFactory.hpp>

// TODO clean me up
namespace Engine
{
namespace LocalTree
{
class SCORE_PLUGIN_ENGINE_EXPORT ProcessComponent
    : public Component<Process::GenericProcessComponent<DocumentPlugin>>
{
  ABSTRACT_COMPONENT_METADATA(
      Engine::LocalTree::ProcessComponent,
      "0732ab51-a052-4e2e-a1f7-9bf2926c199c")
public:
  ProcessComponent(
      ossia::net::node_base& node,
      Process::ProcessModel& proc,
      DocumentPlugin& doc,
      const Id<score::Component>& id,
      const QString& name,
      QObject* parent);

  virtual ~ProcessComponent();
};

template <typename Process_T>
using ProcessComponent_T
    = Process::GenericProcessComponent_T<ProcessComponent, Process_T>;

class SCORE_PLUGIN_ENGINE_EXPORT ProcessComponentFactory
    : public score::
          GenericComponentFactory<Process::ProcessModel, LocalTree::DocumentPlugin, LocalTree::ProcessComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(Engine::LocalTree::ProcessComponent)
public:
  virtual ~ProcessComponentFactory();
  virtual ProcessComponent* make(
      const Id<score::Component>&,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      LocalTree::DocumentPlugin& doc,
      QObject* paren_objt) const = 0;
};

template <typename ProcessComponent_T>
class ProcessComponentFactory_T
    : public score::
          GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
public:
  using model_type = typename ProcessComponent_T::model_type;
  ProcessComponent* make(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      DocumentPlugin& doc,
      QObject* paren_objt) const override
  {
    return new ProcessComponent_T{id, parent, static_cast<model_type&>(proc),
                                  doc, paren_objt};
  }
};

class SCORE_PLUGIN_ENGINE_EXPORT ProcessComponentFactoryList final : public score::
    GenericComponentFactoryList<Process::ProcessModel, LocalTree::DocumentPlugin, LocalTree::ProcessComponentFactory>
{
public:
  ~ProcessComponentFactoryList();
};
}
}
