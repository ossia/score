#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score_plugin_engine_export.h>
namespace Audio::Settings
{
class SCORE_PLUGIN_ENGINE_EXPORT Model
    : public score::SettingsDelegateModel
{
  Q_OBJECT
    Q_PROPERTY(QString CardIn READ getCardIn WRITE setCardIn NOTIFY CardInChanged)
    Q_PROPERTY(QString CardOut READ getCardOut WRITE setCardOut NOTIFY CardOutChanged)
    Q_PROPERTY(int BufferSize READ getBufferSize WRITE setBufferSize NOTIFY BufferSizeChanged)
    Q_PROPERTY(int Rate READ getRate WRITE setRate NOTIFY RateChanged)
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};
public:
  Model(QSettings& set, const score::ApplicationContext& ctx);


  SCORE_SETTINGS_PARAMETER_HPP(QString, CardIn)
  SCORE_SETTINGS_PARAMETER_HPP(QString, CardOut)
  SCORE_SETTINGS_PARAMETER_HPP(int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(int, Rate)
};

SCORE_SETTINGS_PARAMETER(Model, CardIn)
SCORE_SETTINGS_PARAMETER(Model, CardOut)
SCORE_SETTINGS_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_PARAMETER(Model, Rate)
}


