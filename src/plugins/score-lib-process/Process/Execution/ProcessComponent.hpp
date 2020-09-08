#pragma once
#include <Process/ExecutionComponent.hpp>
#include <Process/ExecutionContext.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessComponent.hpp>

#include <score/model/ComponentFactory.hpp>
#include <score/plugins/ModelFactory.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/editor/scenario/time_process.hpp>

#include <QObject>

#include <score_lib_process_export.h>

#include <memory>
#include <verdigris>

namespace ossia
{
class scenario;
class time_process;
}

namespace Execution
{
struct Context;
struct Transaction;
template <typename T>
class InvalidProcessException : public std::runtime_error
{
public:
  InvalidProcessException(const QString& s)
      : std::runtime_error{(Metadata<PrettyName_k, T>::get() + ": " + s).toStdString()}
  {
  }
};

class SCORE_LIB_PROCESS_EXPORT ProcessComponent
    : public Process::GenericProcessComponent<const Context>,
      public std::enable_shared_from_this<ProcessComponent>
{
  W_OBJECT(ProcessComponent)
  ABSTRACT_COMPONENT_METADATA(Execution::ProcessComponent, "d0f714de-c832-42d8-a605-60f5ffd0b7af")

public:
  static constexpr bool is_unique = true;

  ProcessComponent(
      Process::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      const QString& name,
      QObject* parent);

  //! Reimplement this if the element needs two-phase initialization.
  virtual void lazy_init();

  virtual ~ProcessComponent();

  virtual void cleanup();
  virtual void stop() { process().stopExecution(); }

  const std::shared_ptr<ossia::time_process>& OSSIAProcessPtr() { return m_ossia_process; }
  ossia::time_process& OSSIAProcess() const { return *m_ossia_process; }

  std::shared_ptr<ossia::graph_node> node;

public:
  void nodeChanged(
      const ossia::node_ptr& old_node,
      const ossia::node_ptr& new_node,
      Execution::Transaction& commands)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, nodeChanged, old_node, new_node, commands)

protected:
  std::shared_ptr<ossia::time_process> m_ossia_process;
};

template <typename Process_T, typename OSSIA_Process_T>
struct ProcessComponent_T : public Process::GenericProcessComponent_T<ProcessComponent, Process_T>
{
  using Process::GenericProcessComponent_T<ProcessComponent, Process_T>::GenericProcessComponent_T;

  OSSIA_Process_T& OSSIAProcess() const
  {
    return static_cast<OSSIA_Process_T&>(ProcessComponent::OSSIAProcess());
  }
};

class SCORE_LIB_PROCESS_EXPORT ProcessComponentFactory : public score::GenericComponentFactory<
                                                             Process::ProcessModel,
                                                             Execution::Context,
                                                             Execution::ProcessComponentFactory>
{
  SCORE_ABSTRACT_COMPONENT_FACTORY(Execution::ProcessComponent)
public:
  virtual ~ProcessComponentFactory() override;
  virtual std::shared_ptr<ProcessComponent> make(
      Process::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent) const = 0;
};

template <typename ProcessComponent_T>
class ProcessComponentFactory_T
    : public score::GenericComponentFactoryImpl<ProcessComponent_T, ProcessComponentFactory>
{
public:
  using model_type = typename ProcessComponent_T::model_type;
  std::shared_ptr<ProcessComponent> make(
      Process::ProcessModel& proc,
      const Context& ctx,
      const Id<score::Component>& id,
      QObject* parent) const final override
  {
    try
    {
      auto comp
          = std::make_shared<ProcessComponent_T>(static_cast<model_type&>(proc), ctx, id, parent);
      comp->lazy_init();
      return comp;
    }
    catch (const std::runtime_error& e)
    {
      qDebug() << "Error during plug-in creation: " << e.what();
      return {};
    }
    catch (...)
    {
      return {};
    }
  }
};

class SCORE_LIB_PROCESS_EXPORT ProcessComponentFactoryList final
    : public score::GenericComponentFactoryList<
          Process::ProcessModel,
          Execution::Context,
          Execution::ProcessComponentFactory>
{
public:
  ~ProcessComponentFactoryList();
};
}

W_REGISTER_ARGTYPE(ossia::node_ptr)
Q_DECLARE_METATYPE(std::shared_ptr<Execution::ProcessComponent>)
W_REGISTER_ARGTYPE(std::shared_ptr<Execution::ProcessComponent>)
W_REGISTER_ARGTYPE(const std::shared_ptr<Execution::ProcessComponent>&)
W_REGISTER_ARGTYPE(Execution::Transaction&)
