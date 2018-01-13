#pragma once
#include <Process/Process.hpp>
#include <score/model/Component.hpp>

namespace Process
{
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
    = Process::ProcessComponent<score::GenericComponent<System_T>>;

template <typename ProcessComponentBase_T, typename Process_T>
class GenericProcessComponent_T : public ProcessComponentBase_T
{
public:
  using model_type = Process_T;
  using ProcessComponentBase_T::ProcessComponentBase_T;

  Process_T& process() const
  {
    return static_cast<Process_T&>(ProcessComponentBase_T::process());
  }
};
}
