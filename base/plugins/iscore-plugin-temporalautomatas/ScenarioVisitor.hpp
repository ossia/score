#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <QAction>
namespace TemporalAutomatas
{
class ApplicationPlugin : public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(iscore::Application& app);

        void populateMenus(iscore::MenubarManager*) override;

        QAction* m_convert{};
};
}
