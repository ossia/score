#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <QAction>
namespace TemporalAutomatas
{
class ApplicationPlugin : public iscore::PluginControlInterface
{
    public:
        ApplicationPlugin(iscore::Application& app);

        void populateMenus(iscore::MenubarManager*) override;

        QAction* m_convert{};
};
}
