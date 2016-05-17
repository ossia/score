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
        Q_PROPERTY(bool localTree READ getLocalTree WRITE setLocalTree NOTIFY LocalTreeChanged)

    public:
        Model();

        bool getLocalTree() const;
        void setLocalTree(bool);

    signals:
        void LocalTreeChanged(bool);

    private:
        void setFirstTimeSettings() override;
        bool m_tree = false;
};

ISCORE_SETTINGS_DEFERRED_PARAMETER(Model, LocalTree)

}
}
}
