#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
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
};

class ISCORE_PLUGIN_SCENARIO_EXPORT Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(QString m_skin READ getSkin WRITE setSkin NOTIFY skinChanged)
        Q_PROPERTY(double m_graphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY graphicZoomChanged)
        Q_PROPERTY(qreal m_slotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY slotHeightChanged)

    public:
        Model();

        QString getSkin() const;
        void setSkin(const QString&);

        double getGraphicZoom() const;
        void setGraphicZoom(double);

        qreal getSlotHeight() const;
        void setSlotHeight(const qreal& slotHeight);

    signals:
        void skinChanged(const QString&);
        void graphicZoomChanged(double);
        void slotHeightChanged(qreal);

private:
        void setFirstTimeSettings() override;
        QString m_skin;
        double m_graphicZoom;
        qreal m_slotHeight;
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)
ISCORE_SETTINGS_PARAMETER(Model, GraphicZoom)
ISCORE_SETTINGS_PARAMETER(Model, SlotHeight)
}
}
