#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <ossia/dataflow/safe_nodes/node.hpp>
#include <ossia/detail/for_each.hpp>

#include <Control/Widgets.hpp>
#include <Engine/Node/CommonWidgets.hpp>
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Inspector.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>

#define make_uuid(text) score::uuids::string_generator::compute((text))
#if defined(_MSC_VER)
#define uuid_constexpr inline
#else
#define uuid_constexpr constexpr
#endif

namespace Control
{
struct Meta_base : public ossia::safe_nodes::base_metadata
{
  static const constexpr Process::ProcessFlags flags = Process::ProcessFlags::SupportsLasting;
};

template <typename Node>
using ProcessFactory = Process::ProcessFactory_T<ControlProcess<Node>>;

template <typename Node>
struct ExecutorFactory final : public Execution::ProcessComponentFactory_T<Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<Executor<Node>>::ProcessComponentFactory_T;
};

template <typename Node>
using LayerFactory = ControlLayerFactory<Node>;

template <typename... Args>
struct create_types
{
  template <template <typename> typename GenericFactory>
  auto perform()
  {
    std::vector<std::unique_ptr<score::InterfaceBase>> vec;
    ossia::for_each_tagged(brigand::list<Args...>{}, [&](auto t) {
      using type = typename decltype(t)::type;
      vec.emplace_back(std::make_unique<GenericFactory<type>>());
    });
    return vec;
  }
};
template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>>
instantiate_fx(const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
  if (key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::ExecutorFactory>();
  }
  else if (key == Process::ProcessModelFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::ProcessFactory>();
  }
  else if (key == Process::LayerFactory::static_interfaceKey())
  {
    return create_types<Nodes...>{}.template perform<Control::LayerFactory>();
  }
  return {};
}

struct Note
{
  uint8_t pitch{};
  uint8_t vel{};
  uint8_t chan{};
};

template <typename T>
struct score_generic_plugin final : public score::FactoryInterface_QtInterface,
                                    public score::Plugin_QtInterface
{
  static MSVC_BUGGY_CONSTEXPR score::PluginKey static_key() { return T::Metadata::uuid; }

  score::PluginKey key() const final override { return static_key(); }

  score::Version version() const override { return score::Version{1}; }

  score_generic_plugin() = default;
  ~score_generic_plugin() override = default;

  std::vector<std::unique_ptr<score::InterfaceBase>>
  factories(const score::ApplicationContext& ctx, const score::InterfaceKey& key) const override
  {
    return Control::instantiate_fx<T>(ctx, key);
  }

  std::vector<score::PluginKey> required() const override { return {}; }
};
}
