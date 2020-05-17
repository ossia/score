// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_ui.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <Engine/Node/PdNode.hpp>
#include <Ui/SignalDisplay.hpp>
#include <score_plugin_engine.hpp>

score_plugin_ui::score_plugin_ui() = default;
score_plugin_ui::~score_plugin_ui() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_ui::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return Control::instantiate_fx<Ui::SignalDisplay::Node>(ctx, key);
}

auto score_plugin_ui::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_ui)
