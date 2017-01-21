#pragma once
#include <Process/TimeValue.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
namespace Settings
{
class ISCORE_PLUGIN_SCENARIO_EXPORT Model final
    : public iscore::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(QString Skin READ getSkin WRITE setSkin NOTIFY SkinChanged FINAL)
  Q_PROPERTY(double GraphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY
                 GraphicZoomChanged FINAL)
  Q_PROPERTY(qreal SlotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY
                 SlotHeightChanged FINAL)
  Q_PROPERTY(TimeVal DefaultDuration READ getDefaultDuration WRITE
                 setDefaultDuration NOTIFY DefaultDurationChanged FINAL)
  Q_PROPERTY(bool SnapshotOnCreate READ getSnapshotOnCreate WRITE
                 setSnapshotOnCreate NOTIFY SnapshotOnCreateChanged FINAL)
  Q_PROPERTY(bool AutoSequence READ getAutoSequence WRITE setAutoSequence
                 NOTIFY AutoSequenceChanged FINAL)

  QString m_Skin;
  double m_GraphicZoom{};
  qreal m_SlotHeight{};
  TimeVal m_DefaultDuration{};
  bool m_SnapshotOnCreate{};
  bool m_AutoSequence{};

public:
  Model(QSettings& set, const iscore::ApplicationContext& ctx);

  QString getSkin() const;
  void setSkin(const QString&);

  ISCORE_SETTINGS_PARAMETER_HPP(double, GraphicZoom)
  ISCORE_SETTINGS_PARAMETER_HPP(qreal, SlotHeight)
  ISCORE_SETTINGS_PARAMETER_HPP(TimeVal, DefaultDuration)
  ISCORE_SETTINGS_PARAMETER_HPP(bool, SnapshotOnCreate)
  ISCORE_SETTINGS_PARAMETER_HPP(bool, AutoSequence)

signals:
  void SkinChanged(const QString&);
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)
ISCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
ISCORE_SETTINGS_PARAMETER(Model, SlotHeight)
ISCORE_SETTINGS_PARAMETER(Model, DefaultDuration)
ISCORE_SETTINGS_PARAMETER(Model, SnapshotOnCreate)
ISCORE_SETTINGS_PARAMETER(Model, AutoSequence)
}
}
