#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>


namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

struct Keys
{
        static const QString localTree;
};

class Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(bool localTree READ getLocalTree WRITE setLocalTree NOTIFY localTreeChanged)

    public:
        Model();

        bool getLocalTree() const;
        void setLocalTree(bool);

    signals:
        void localTreeChanged(bool);

    private:
        void setFirstTimeSettings() override;
        bool m_tree = false;
};

ISCORE_SETTINGS_PARAMETER(Model, LocalTree)

}
}
}
