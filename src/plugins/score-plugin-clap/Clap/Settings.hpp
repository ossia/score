#pragma once
#include <Media/Effect/Settings/View.hpp>

#include <score/command/Dispatchers/SettingsCommandDispatcher.hpp>

namespace Clap
{
//! "CLAP" tab of the Effects settings page: search paths + scanned plug-ins.
class SettingsWidget final : public Media::Settings::PluginSettingsTab
{
  SCORE_CONCRETE("4a1022f6-32f7-49ff-bcf1-42f115394340")
public:
  QString name() const noexcept override;
  QWidget* make(const score::ApplicationContext& ctx) override;

private:
  score::SettingsCommandDispatcher m_disp;
};
}
