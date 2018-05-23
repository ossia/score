#pragma once
#include <Process/TimeValue.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
namespace Settings
{
class SCORE_PLUGIN_SCENARIO_EXPORT Model final
    : public score::SettingsDelegateModel
{
  W_OBJECT(Model)
  QString m_Skin;
  QString m_DefaultEditor;
  double m_GraphicZoom{};
  qreal m_SlotHeight{};
  TimeVal m_DefaultDuration{std::chrono::seconds{30}};
  bool m_SnapshotOnCreate{};
  bool m_AutoSequence{};
  bool m_TimeBar{};

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  QString getSkin() const;
  void setSkin(const QString&);

  SCORE_SETTINGS_PARAMETER_HPP(QString, DefaultEditor)
  SCORE_SETTINGS_PARAMETER_HPP(double, GraphicZoom)
  SCORE_SETTINGS_PARAMETER_HPP(qreal, SlotHeight)
  SCORE_SETTINGS_PARAMETER_HPP(TimeVal, DefaultDuration)
  SCORE_SETTINGS_PARAMETER_HPP(bool, SnapshotOnCreate)
  SCORE_SETTINGS_PARAMETER_HPP(bool, AutoSequence)
  SCORE_SETTINGS_PARAMETER_HPP(bool, TimeBar)

public:
  void SkinChanged(const QString& arg_1) W_SIGNAL(SkinChanged, arg_1);
  PROPERTY(QString, Skin READ getSkin WRITE setSkin NOTIFY SkinChanged, W_Final)
};

SCORE_SETTINGS_PARAMETER(Model, DefaultEditor)
SCORE_SETTINGS_PARAMETER(Model, Skin)
SCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
SCORE_SETTINGS_PARAMETER(Model, SlotHeight)
SCORE_SETTINGS_PARAMETER(Model, DefaultDuration)
SCORE_SETTINGS_PARAMETER(Model, SnapshotOnCreate)
SCORE_SETTINGS_PARAMETER(Model, AutoSequence)
SCORE_SETTINGS_PARAMETER(Model, TimeBar)
}
}
