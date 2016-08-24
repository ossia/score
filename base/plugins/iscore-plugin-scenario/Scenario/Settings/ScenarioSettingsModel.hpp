#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <Process/TimeValue.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
namespace Settings
{
class ISCORE_PLUGIN_SCENARIO_EXPORT Model final :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(QString Skin READ getSkin WRITE setSkin NOTIFY SkinChanged FINAL)
        Q_PROPERTY(double GraphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY GraphicZoomChanged FINAL)
        Q_PROPERTY(qreal SlotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY SlotHeightChanged FINAL)
        Q_PROPERTY(TimeValue DefaultDuration READ getDefaultDuration WRITE setDefaultDuration NOTIFY DefaultDurationChanged FINAL)

        QString m_Skin;
        double m_GraphicZoom;
        qreal m_SlotHeight;
        TimeValue m_DefaultDuration;

    public:
        Model(QSettings& set, const iscore::ApplicationContext& ctx);

        QString getSkin() const;
        void setSkin(const QString&);

        ISCORE_SETTINGS_PARAMETER_HPP(double, GraphicZoom)
        ISCORE_SETTINGS_PARAMETER_HPP(qreal, SlotHeight)
        ISCORE_SETTINGS_PARAMETER_HPP(TimeValue, DefaultDuration)

    signals:
        void SkinChanged(const QString&);
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)
ISCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
ISCORE_SETTINGS_PARAMETER(Model, SlotHeight)
ISCORE_SETTINGS_PARAMETER(Model, DefaultDuration)
}
}
