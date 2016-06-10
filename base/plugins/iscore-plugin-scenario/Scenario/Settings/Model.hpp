#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <Process/TimeValue.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
namespace Settings
{

struct Keys
{
        static const QString skin;
        static const QString graphicZoom;
        static const QString slotHeight;
        static const QString defaultDuration;
};

class ISCORE_PLUGIN_SCENARIO_EXPORT Model final :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(QString m_skin READ getSkin WRITE setSkin NOTIFY SkinChanged)
        Q_PROPERTY(double m_graphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY GraphicZoomChanged)
        Q_PROPERTY(qreal m_slotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY SlotHeightChanged)
        Q_PROPERTY(TimeValue m_defaultDuration READ getDefaultScoreDuration WRITE setDefaultScoreDuration NOTIFY DefaultScoreDurationChanged)

    public:
        Model(const iscore::ApplicationContext& ctx);

        QString getSkin() const;
        void setSkin(const QString&);

        double getGraphicZoom() const;
        void setGraphicZoom(double);

        qreal getSlotHeight() const;
        void setSlotHeight(qreal slotHeight);

        TimeValue getDefaultScoreDuration();
        void setDefaultScoreDuration(const TimeValue&);

    signals:
        void SkinChanged(const QString&);
        void GraphicZoomChanged(double);
        void SlotHeightChanged(qreal);
        void DefaultScoreDurationChanged(const TimeValue&);

    private:
        void setFirstTimeSettings() override;
        QString m_skin;
        double m_graphicZoom;
        qreal m_slotHeight;
        TimeValue m_defaultDuration;
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)
ISCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
ISCORE_SETTINGS_PARAMETER(Model, SlotHeight)
ISCORE_SETTINGS_PARAMETER(Model, DefaultScoreDuration)
}
}
