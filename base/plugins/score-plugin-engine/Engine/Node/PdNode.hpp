#pragma once
#include <Engine/Node/Widgets.hpp>
#include <Engine/Node/Node.hpp>
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Inspector.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Engine/Node/Effect.hpp>
#include <brigand/algorithms/for_each.hpp>
#include <Process/GenericProcessFactory.hpp>
#define make_uuid(text) score::uuids::string_generator::compute((text))

namespace Control
{
template<typename Node>
using ProcessFactory = Process::GenericProcessModelFactory<ControlProcess<Node>>;
template<typename Node>
using ExecutorFactory = Engine::Execution::ProcessComponentFactory_T<Executor<Node>>;
template<typename Node>
using LayerFactory = ControlLayerFactory<Node>;

template<typename Node>
using EffectFactory = Process::EffectFactory_T<ControlEffect<Node>>;
template<typename Node>
using EffectExecutorFactory = Engine::Execution::EffectComponentFactory_T<Engine::Execution::ControlEffectComponent<Node>>;
template<typename Node>
using EffectUIFactory = Process::EffectUIFactory_T<ControlEffect<Node>, ControlEffectView<Node>>;


template<typename... Args>
struct create_types
{
    template<template<typename> typename GenericFactory>
    auto perform()
    {
      std::vector<std::unique_ptr<score::InterfaceBase>> vec;
      brigand::for_each<brigand::list<Args...>>([&] (auto t){
        using type = typename decltype(t)::type;
        vec.emplace_back(std::make_unique<GenericFactory<type>>());
      });
      return vec;
    }
};
template<typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>> instantiate_fx(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  if(key == Engine::Execution::ProcessComponentFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::ExecutorFactory>();
  }
  else if(key == Process::ProcessModelFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::ProcessFactory>();
  }
  else if(key == Process::EffectFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::EffectFactory>();
  }
  else if(key == Process::EffectUIFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::EffectUIFactory>();
  }
  else if(key == Process::InspectorWidgetDelegateFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::InspectorFactory>();
  }
  else if(key == Process::LayerFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::LayerFactory>();
  }
  else if(key == Engine::Execution::EffectComponentFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::EffectExecutorFactory>();
  }
  return {};
}
}

