#pragma once
#include <Process/TimeValue.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
namespace Settings
{
class SCORE_PLUGIN_SCENARIO_EXPORT Model final
    : public score::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(QString Skin READ getSkin WRITE setSkin NOTIFY SkinChanged FINAL)
  Q_PROPERTY(double GraphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY GraphicZoomChanged FINAL)
  Q_PROPERTY(qreal SlotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY SlotHeightChanged FINAL)
  Q_PROPERTY(TimeVal DefaultDuration READ getDefaultDuration WRITE setDefaultDuration NOTIFY DefaultDurationChanged FINAL)
  Q_PROPERTY(bool SnapshotOnCreate READ getSnapshotOnCreate WRITE setSnapshotOnCreate NOTIFY SnapshotOnCreateChanged FINAL)
  Q_PROPERTY(bool AutoSequence READ getAutoSequence WRITE setAutoSequence NOTIFY AutoSequenceChanged FINAL)
  Q_PROPERTY(bool TimeBar READ getTimeBar WRITE setTimeBar NOTIFY TimeBarChanged FINAL)

  QString m_Skin;
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

  SCORE_SETTINGS_PARAMETER_HPP(double, GraphicZoom)
  SCORE_SETTINGS_PARAMETER_HPP(qreal, SlotHeight)
  SCORE_SETTINGS_PARAMETER_HPP(TimeVal, DefaultDuration)
  SCORE_SETTINGS_PARAMETER_HPP(bool, SnapshotOnCreate)
  SCORE_SETTINGS_PARAMETER_HPP(bool, AutoSequence)
  SCORE_SETTINGS_PARAMETER_HPP(bool, TimeBar)

Q_SIGNALS:
  void SkinChanged(const QString&);
};

SCORE_SETTINGS_PARAMETER(Model, Skin)
SCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
SCORE_SETTINGS_PARAMETER(Model, SlotHeight)
SCORE_SETTINGS_PARAMETER(Model, DefaultDuration)
SCORE_SETTINGS_PARAMETER(Model, SnapshotOnCreate)
SCORE_SETTINGS_PARAMETER(Model, AutoSequence)
SCORE_SETTINGS_PARAMETER(Model, TimeBar)
}
}
