#pragma once
#include <Process/Dataflow/ControlWidgets.hpp>

#include <Crousti/Executor.hpp>
#include <Crousti/Layer.hpp>
#include <Crousti/ProcessModel.hpp>
#include <Crousti/ScoreLayer.hpp>
#include <Dataflow/WidgetInletFactory.hpp>

#include <ossia/detail/for_each.hpp>

#include <Avnd/Controls.hpp>

namespace oscr
{
template <typename Node>
struct ProcessFactory final : public Process::ProcessFactory_T<oscr::ProcessModel<Node>>
{
  using Process::ProcessFactory_T<oscr::ProcessModel<Node>>::ProcessFactory_T;
};

template <typename Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<oscr::Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<
      oscr::Executor<Node>>::ProcessComponentFactory_T;
};

template <typename Node>
static void instantiate_fx(
    std::vector<score::InterfaceBase*>& v,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  if(key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    //static_assert((requires { std::declval<Nodes>().run({}, {}); } && ...));
    v.emplace_back(
        static_cast<Execution::ProcessComponentFactory*>(
            new oscr::ExecutorFactory<Node>()));
  }
  else if(key == Process::ProcessModelFactory::static_interfaceKey())
  {
    v.emplace_back(
        static_cast<Process::ProcessModelFactory*>(new oscr::ProcessFactory<Node>()));
  }
  else if(key == Process::LayerFactory::static_interfaceKey())
  {
      if constexpr(avnd::has_ui<Node>)
      {
        v.emplace_back(
            static_cast<Process::LayerFactory*>(new oscr::LayerFactory<Node>()));
      }
      else if constexpr(oscr::has_ossia_layer<Node>)
      {
        v.emplace_back(
            static_cast<Process::LayerFactory*>(new oscr::ScoreLayerFactory<Node>()));
      }
   }
  else if(key == Process::PortFactory::static_interfaceKey())
  {
    // Go through all the process's control inputs with a mapper
    // Generate the matching WidgetInletFactory
    using namespace boost::mp11;

    auto fun = [&]<typename N, typename... Fields>(avnd::typelist<Fields...>) {
      (v.emplace_back(
           static_cast<Process::PortFactory*>(new CustomControlFactory<N, Fields>{})),
       ...);
    };
    fun.template operator()<Node>(reflect_mapped_controls<Node>{});
    fun.template operator()<Node>(reflect_controller_controls<Node>{});
  }
}

template <typename T>
void custom_factories(
    std::vector<score::InterfaceBase*>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key);

}

namespace Avnd = oscr;
