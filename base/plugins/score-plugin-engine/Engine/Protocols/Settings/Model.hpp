#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score_plugin_engine_export.h>
namespace Audio::Settings
{
class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)
  
  
  
  
  
  
  

  QString m_Driver{};
  QString m_CardIn{};
  QString m_CardOut{};
  int m_BufferSize{};
  int m_Rate{};
  int m_DefaultIn{};
  int m_DefaultOut{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(QString, Driver)
  SCORE_SETTINGS_PARAMETER_HPP(QString, CardIn)
  SCORE_SETTINGS_PARAMETER_HPP(QString, CardOut)
  SCORE_SETTINGS_PARAMETER_HPP(int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(int, DefaultIn)
  SCORE_SETTINGS_PARAMETER_HPP(int, DefaultOut)

W_PROPERTY(int, DefaultOut READ getDefaultOut WRITE setDefaultOut NOTIFY DefaultOutChanged)

W_PROPERTY(int, DefaultIn READ getDefaultIn WRITE setDefaultIn NOTIFY DefaultInChanged)

W_PROPERTY(int, Rate READ getRate WRITE setRate NOTIFY RateChanged)

W_PROPERTY(int, BufferSize READ getBufferSize WRITE setBufferSize NOTIFY BufferSizeChanged)

W_PROPERTY(QString, CardOut READ getCardOut WRITE setCardOut NOTIFY CardOutChanged)

W_PROPERTY(QString, CardIn READ getCardIn WRITE setCardIn NOTIFY CardInChanged)

W_PROPERTY(QString, Driver READ getDriver WRITE setDriver NOTIFY DriverChanged)
};

SCORE_SETTINGS_PARAMETER(Model, Driver)
SCORE_SETTINGS_PARAMETER(Model, CardIn)
SCORE_SETTINGS_PARAMETER(Model, CardOut)
SCORE_SETTINGS_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, DefaultIn)
SCORE_SETTINGS_PARAMETER(Model, DefaultOut)
}
