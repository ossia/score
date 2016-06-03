#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>


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
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(bool localTree READ getLocalTree WRITE setLocalTree NOTIFY LocalTreeChanged)

    public:
        Model(const iscore::ApplicationContext& ctx);

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
