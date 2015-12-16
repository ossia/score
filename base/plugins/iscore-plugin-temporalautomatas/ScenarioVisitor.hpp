#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class QAction;
namespace iscore {

class MenubarManager;
}  // namespace iscore
// RENAMEME
namespace TemporalAutomatas
{
class ApplicationPlugin :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(const iscore::ApplicationContext& app);

        void populateMenus(iscore::MenubarManager*) override;

        QAction* m_generate{};
        QAction* m_convert{};
        QAction* m_metrics{};
};
}
