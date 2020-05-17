#pragma once
#include <Process/ProcessComponent.hpp>

#include <score/model/Component.hpp>
#include <score/model/ComponentFactory.hpp>
#include <score/plugins/ModelFactory.hpp>

#include <LocalTree/LocalTreeComponent.hpp>
#include <score_lib_process_export.h>
namespace LocalTree
{
class SCORE_LIB_PROCESS_EXPORT ProcessComponent
    : public Component<Process::GenericProcessComponent<const score::DocumentContext>>
{
  ABSTRACT_COMPONENT_METADATA(LocalTree::ProcessComponent, "0732ab51-a052-4e2e-a1f7-9bf2926c199c")
public:
  ProcessComponent(
      ossia::net::node_base& node,
      Process::ProcessModel& proc,
      const score::DocumentContext& doc,
      const Id<score::Component>& id,
      const QString& name,
      QObject* parent);

  virtual ~ProcessComponent();
};

template <typename Process_T>
using ProcessComponent_T = Process::GenericProcessComponent_T<ProcessComponent, Process_T>;

class SCORE_LIB_PROCESS_EXPORT ProcessComponentFactory : public score::GenericComponentFactory<
                                                             Process::ProcessModel,
                                                             score::DocumentContext,
                                                             LocalTree::ProcessComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(LocalTree::ProcessComponent)
public:
  virtual ~ProcessComponentFactory();
  virtual ProcessComponent* make(
      const Id<score::Component>&,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      const score::DocumentContext& doc,
      QObject* paren_objt) const = 0;
};

template <typename ProcessComponent_T>
class ProcessComponentFactory_T
    : public score::GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
public:
  using model_type = typename ProcessComponent_T::model_type;
  ProcessComponent* make(
      const Id<score::Component>& id,
      ossia::net::node_base& parent,
      Process::ProcessModel& proc,
      const score::DocumentContext& doc,
      QObject* paren_objt) const override
  {
    return new ProcessComponent_T{id, parent, static_cast<model_type&>(proc), doc, paren_objt};
  }
};

class SCORE_LIB_PROCESS_EXPORT ProcessComponentFactoryList final
    : public score::GenericComponentFactoryList<
          Process::ProcessModel,
          score::DocumentContext,
          LocalTree::ProcessComponentFactory>
{
public:
  ~ProcessComponentFactoryList();
};
}
