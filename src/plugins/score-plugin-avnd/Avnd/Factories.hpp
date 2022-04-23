#pragma once
#include <Crousti/ProcessModel.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/Layer.hpp>

#include <ossia/detail/for_each.hpp>

namespace oscr
{
template <typename Node>
struct ProcessFactory final
    : public Process::ProcessFactory_T<oscr::ProcessModel<Node>>
{
  using Process::ProcessFactory_T<oscr::ProcessModel<Node>>::ProcessFactory_T;
};

template <typename Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<oscr::Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<oscr::Executor<Node>>::ProcessComponentFactory_T;
};

template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>> instantiate_fx(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  std::vector<std::unique_ptr<score::InterfaceBase>> v;

  if (key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    //static_assert((requires { std::declval<Nodes>().run({}, {}); } && ...));
    (v.emplace_back(static_cast<Execution::ProcessComponentFactory*>(new oscr::ExecutorFactory<Nodes>())), ...);
  }
  else if (key == Process::ProcessModelFactory::static_interfaceKey())
  {
    (v.emplace_back(static_cast<Process::ProcessModelFactory*>(new oscr::ProcessFactory<Nodes>())), ...);
  }
  else if (key == Process::LayerFactory::static_interfaceKey())
  {
    auto fun = [&] <typename type> () {
      if constexpr(avnd::has_ui<type>)
      {
        v.emplace_back(static_cast<Process::LayerFactory*>(new oscr::LayerFactory<type>()));
      }
    };
    (fun.template operator()<Nodes>(), ...);
  }

  return v;
}
}

namespace Avnd = oscr;
