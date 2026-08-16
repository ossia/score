#pragma once
#include <Media/Effect/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

namespace LV2
{
//! "LV2" tab of the Effects settings page: search paths + scanned plug-ins.
class SettingsWidget final : public Media::Settings::PluginSettingsTab
{
  SCORE_CONCRETE("5c729f84-fa4b-4d14-9951-ecab29ec5886")
public:
  QString name() const noexcept override;
  QWidget* make(const score::ApplicationContext& ctx) override;

private:
  score::SettingsCommandDispatcher m_disp;
};
}
