#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

class QAction;
namespace iscore {
class Application;
class MenubarManager;
}  // namespace iscore

namespace TemporalAutomatas
{
class ApplicationPlugin :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(iscore::Application& app);

        void populateMenus(iscore::MenubarManager*) override;

        QAction* m_convert{};
};
}
