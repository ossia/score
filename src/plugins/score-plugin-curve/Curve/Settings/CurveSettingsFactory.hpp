#pragma once
#include <Curve/Settings/CurveSettingsModel.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

namespace Curve
{
namespace Settings
{
/**
 * The curve settings have no page of their own: they are shown on the
 * Recording page (score-plugin-recording), whose recordings of addresses they
 * set up, beside the simplification the Pen tool uses.
 */
class Factory final : public score::SettingsDelegateFactory
{
  SCORE_CONCRETE("aa38303e-edbf-4c77-ac4d-5e335df73bca")

  std::unique_ptr<score::SettingsDelegateModel>
  makeModel(QSettings& settings, const score::ApplicationContext& ctx) override
  {
    return std::make_unique<Model>(concreteKey(), settings, ctx);
  }
  score::GlobalSettingsView* makeView() override { return nullptr; }
  score::GlobalSettingsPresenter* makePresenter_impl(
      score::SettingsDelegateModel&, score::GlobalSettingsView&, QObject*) override
  {
    return nullptr;
  }
};
}
}
