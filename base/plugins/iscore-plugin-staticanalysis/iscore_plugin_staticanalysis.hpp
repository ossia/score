#pragma once
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>
#include <QObject>

#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

namespace iscore {

}  // namespace iscore

namespace TA
{
}
class iscore_addon_staticanalysis final:
        public QObject,
        public iscore::GUIApplicationContextPlugin_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID GUIApplicationContextPlugin_QtInterface_iid)
        Q_INTERFACES(
                iscore::GUIApplicationContextPlugin_QtInterface
                )

    public:
        iscore_addon_staticanalysis();
        virtual ~iscore_addon_staticanalysis() = default;

        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                const iscore::ApplicationContext& app) override;

};
