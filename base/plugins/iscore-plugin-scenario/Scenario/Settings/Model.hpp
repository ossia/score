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

class ISCORE_PLUGIN_SCENARIO_EXPORT Model final :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(QString m_skin READ getSkin WRITE setSkin NOTIFY SkinChanged)
        Q_PROPERTY(double m_graphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY GraphicZoomChanged)
        Q_PROPERTY(qreal m_slotHeight READ getSlotHeight WRITE setSlotHeight NOTIFY SlotHeightChanged)

    public:
        Model(const iscore::ApplicationContext& ctx);

        QString getSkin() const;
        void setSkin(const QString&);

        double getGraphicZoom() const;
        void setGraphicZoom(double);

        qreal getSlotHeight() const;
        void setSlotHeight(const qreal& slotHeight);

    signals:
        void SkinChanged(const QString&);
        void GraphicZoomChanged(double);
        void SlotHeightChanged(qreal);

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
