// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_ui.hpp"

#include <Engine/Node/SimpleApi.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <Avnd/Factories.hpp>
#include <Ui/SignalDisplay.hpp>
#include <Ui/TextBox.hpp>
#include <Ui/ValueDisplay.hpp>
#include <Ui/VUMeter.hpp>

#include <score_plugin_engine.hpp>

score_plugin_ui::score_plugin_ui() = default;
score_plugin_ui::~score_plugin_ui() = default;

std::vector<score::InterfaceBase*> score_plugin_ui::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  std::vector<score::InterfaceBase*> fx;
  oscr::instantiate_fx<Ui::SignalDisplay::Node>(fx, ctx, key);
  oscr::instantiate_fx<Ui::TextBox::Node>(fx, ctx, key);
  oscr::instantiate_fx<Ui::ValueDisplay::Node>(fx, ctx, key);
  oscr::instantiate_fx<Ui::VUMeter::Node>(fx, ctx, key);
  return fx;
}

auto score_plugin_ui::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_ui)
