#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score_plugin_media_export.h>
namespace Media::Settings
{
class SCORE_PLUGIN_MEDIA_EXPORT Model
    : public score::SettingsDelegateModel
{
  Q_OBJECT
    Q_PROPERTY(QStringList VstPaths READ getVstPaths WRITE setVstPaths NOTIFY VstPathsChanged)
    Q_PROPERTY(QString Card READ getCard WRITE setCard NOTIFY CardChanged)
  QStringList m_VstPaths;

  QString m_Card{};
  int m_BufferSize{};
int m_Rate{};
public:
  Model(QSettings& set, const score::ApplicationContext& ctx);


  SCORE_SETTINGS_PARAMETER_HPP(QStringList, VstPaths)

  SCORE_SETTINGS_PARAMETER_HPP(QString, Card)
  SCORE_SETTINGS_PARAMETER_HPP(int, BufferSize)
  SCORE_SETTINGS_PARAMETER_HPP(int, Rate)
};

SCORE_SETTINGS_PARAMETER(Model, VstPaths)
SCORE_SETTINGS_PARAMETER(Model, Card)
SCORE_SETTINGS_PARAMETER(Model, BufferSize)
SCORE_SETTINGS_PARAMETER(Model, Rate)
}


