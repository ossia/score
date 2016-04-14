#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>


namespace Scenario
{
namespace Settings
{

struct Keys
{
        static const QString skin;
        static const QString graphicZoom;
};

class Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(QString rate READ getSkin WRITE setSkin NOTIFY skinChanged)
        Q_PROPERTY(double m_graphicZoom READ getGraphicZoom WRITE setGraphicZoom NOTIFY graphicZoomChanged)

    public:
        Model();

        QString getSkin() const;
        void setSkin(const QString&);

        double getGraphicZoom() const;
        void setGraphicZoom(double);

    signals:
        void skinChanged(const QString&);
        void graphicZoomChanged(double);

    private:
        void setFirstTimeSettings() override;
        QString m_skin;
        double m_graphicZoom;
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)
ISCORE_SETTINGS_PARAMETER(Model, GraphicZoom)

}
}
