#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>


namespace Scenario
{
namespace Settings
{

struct Keys
{
        static const QString skin;
};

class Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(QString rate READ getSkin WRITE setSkin NOTIFY skinChanged)

    public:
        Model();

        QString getSkin() const;
        void setSkin(const QString&);

    signals:
        void skinChanged(const QString&);

    private:
        void setFirstTimeSettings() override;
        QString m_skin;
};

ISCORE_SETTINGS_PARAMETER(Model, Skin)

}
}
