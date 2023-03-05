#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Control/Widgets.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Inspector.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>

#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <ossia/detail/concepts.hpp>
#include <ossia/detail/for_each.hpp>

#define make_uuid(text) score::uuids::string_generator::compute((text))
#if defined(_MSC_VER)
#define uuid_constexpr inline
#else
#define uuid_constexpr constexpr
#endif

namespace Control
{

template <typename Item>
concept HasItem = requires { &Item::item; } || requires { Item{}.item(0); }
                  || requires { sizeof(typename Item::Layer); };

struct Meta_base : public ossia::safe_nodes::base_metadata
{
  static const constexpr Process::ProcessFlags flags = Process::ProcessFlags(
      Process::ProcessFlags::SupportsLasting | Process::ProcessFlags::ControlSurface);
};

template <typename Node>
using ProcessFactory = Process::ProcessFactory_T<ControlProcess<Node>>;

template <typename Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<Executor<Node>>::ProcessComponentFactory_T;
};

template <typename Node>
using LayerFactory = ControlLayerFactory<Node>;

template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>>
instantiate_fx(const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  std::vector<std::unique_ptr<score::InterfaceBase>> vec;
  if(key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    (vec.emplace_back(
         (Execution::ProcessComponentFactory*)new Control::ExecutorFactory<Nodes>()),
     ...);
  }
  else if(key == Process::ProcessModelFactory::static_interfaceKey())
  {
    (vec.emplace_back(
         (Process::ProcessModelFactory*)new Control::ProcessFactory<Nodes>()),
     ...);
  }
  else if(key == Process::LayerFactory::static_interfaceKey())
  {
    ossia::for_each_tagged(brigand::list<Nodes...>{}, [&](auto t) {
      using type = typename decltype(t)::type;
      if constexpr(HasItem<type>)
      {
        vec.emplace_back((Process::LayerFactory*)new Control::LayerFactory<type>());
      }
    });
  }
  return vec;
}

struct Note
{
  uint8_t pitch{};
  uint8_t vel{};
  uint8_t chan{};
};

template <typename T>
struct score_generic_plugin final
    : public score::FactoryInterface_QtInterface
    , public score::Plugin_QtInterface
{
  static constexpr score::PluginKey static_key() { return T::Metadata::uuid; }

  constexpr score::PluginKey key() const final override { return static_key(); }

  score::Version version() const override { return score::Version{1}; }

  score_generic_plugin() = default;
  ~score_generic_plugin() override = default;

  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx,
      const score::InterfaceKey& key) const override
  {
    return Control::instantiate_fx<T>(ctx, key);
  }

  std::vector<score::PluginKey> required() const override { return {}; }
};
}
