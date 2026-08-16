#pragma once
#include <Media/Effect/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

namespace vst3
{
//! "VST3" tab of the Effects settings page: search paths + scanned plug-ins.
class SettingsWidget final : public Media::Settings::PluginSettingsTab
{
  SCORE_CONCRETE("ac291b3b-b0ed-4a37-ab55-5092337b3e24")
public:
  QString name() const noexcept override;
  QWidget* make(const score::ApplicationContext& ctx) override;

private:
  score::SettingsCommandDispatcher m_disp;
};
}
