#pragma once
#include <iscore/application/ApplicationContext.hpp>

class QMainWindow;
namespace iscore
{
/**
 * @brief Specializes ApplicationContext with the QMainWindow
 *
 * We want to keep it separate in case we do a completely different UI
 * not based on Qt widgets in a few years, but instead on QQuickWindow or even
 * a web ui if it has gotten fast enough.
 */
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
      QMainWindow& mw)
      : iscore::ApplicationContext{a, b, c, d, e, f, g}, mainWindow{mw}
  {
  }

  QMainWindow& mainWindow;
};
}
