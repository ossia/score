#pragma once
#include <iscore/application/ApplicationContext.hpp>

class QMainWindow;
namespace iscore
{
struct GUIApplicationContext : public iscore::ApplicationContext
{
        explicit GUIApplicationContext(
                const iscore::ApplicationSettings& a,
                const ApplicationComponents& b,
                DocumentManager& c,
                iscore::MenuManager& d,
                iscore::ToolbarManager& e,
                iscore::ActionManager& f,
                const std::vector<std::unique_ptr<iscore::SettingsDelegateModel>>& g,
                QMainWindow& mw):
            iscore::ApplicationContext{a, b, c, d, e, f, g},
            mainWindow{mw}
        {

        }

        QMainWindow& mainWindow;
};
}
