#pragma once
#include <Process/Process.hpp>
#include <Process/StateProcess.hpp>
#include <iscore/model/Component.hpp>

namespace Scenario
{
// TODO namespace process instead?

template <typename ProcessBase_T, typename Component_T>
class ProcessComponentBase : public Component_T
{
public:
  template <typename... Args>
  ProcessComponentBase(ProcessBase_T& cst, Args&&... args)
      : Component_T{std::forward<Args>(args)...}, m_process{cst}
  {
  }

  ProcessBase_T& process() const
  {
    return m_process;
  }

private:
  ProcessBase_T& m_process;
};

template <typename Component_T>
using ProcessComponent
    = ProcessComponentBase<Process::ProcessModel, Component_T>;

template <typename System_T>
using GenericProcessComponent
    = Scenario::ProcessComponent<iscore::GenericComponent<System_T>>;

template <typename Component_T>
using StateProcessComponent
    = ProcessComponentBase<Process::StateProcess, Component_T>;

template <typename System_T>
using GenericStateProcessComponent
    = Scenario::StateProcessComponent<iscore::GenericComponent<System_T>>;

template <typename ProcessComponentBase_T, typename Process_T>
class GenericProcessComponent_T : public ProcessComponentBase_T
{
public:
  using model_type = Process_T;
  using ProcessComponentBase_T::ProcessComponentBase_T;

  const Process_T& process() const
  {
    return static_cast<const Process_T&>(ProcessComponentBase_T::process());
  }
};
}
