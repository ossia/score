#pragma once
#include <Engine/Node/Widgets.hpp>
#include <Engine/Node/Node.hpp>
#include <Engine/Node/Process.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Inspector.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <brigand/algorithms/for_each.hpp>
#include <Process/GenericProcessFactory.hpp>
#define make_uuid(text) score::uuids::string_generator::compute((text))

namespace Control
{
struct Meta_base
{
    static const constexpr Control::dummy_container<Control::ValueInInfo> value_ins{};
    static const constexpr Control::dummy_container<Control::ValueOutInfo> value_outs{};
    static const constexpr Control::dummy_container<Control::AudioInInfo> audio_ins{};
    static const constexpr Control::dummy_container<Control::AudioOutInfo> audio_outs{};
    static const constexpr Control::dummy_container<Control::MidiInInfo> midi_ins{};
    static const constexpr Control::dummy_container<Control::MidiOutInfo> midi_outs{};
    static const constexpr Control::dummy_container<Control::AddressInInfo> address_ins{};
    static const constexpr std::tuple<> controls{};
};
template<typename Node>
using ProcessFactory = Process::ProcessFactory_T<ControlProcess<Node>>;

template<typename Node>
struct ExecutorFactory final: public Engine::Execution::ProcessComponentFactory_T<Executor<Node>>
{ using Engine::Execution::ProcessComponentFactory_T<Executor<Node>>::ProcessComponentFactory_T; };

template<typename Node>
using LayerFactory = ControlLayerFactory<Node>;

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
  else if(key == Process::InspectorWidgetDelegateFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::InspectorFactory>();
  }
  else if(key == Process::LayerFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::LayerFactory>();
  }
  return {};
}
}

