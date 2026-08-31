#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QFileInfo>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>

#include <thread>
#include <vector>

namespace ossia::net
{
struct network_context;
using network_context_ptr = std::shared_ptr<network_context>;
}
class QQuickWindow;
namespace JS
{
class ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& ctx);

  void on_createdDocument(score::Document& doc) override;

  ~ApplicationPlugin() override;
  void afterStartup() override;
  void on_newDocument(score::Document& doc) override;

  /** Load an ES module and give it a chance to set itself up.
   *
   * ES modules cannot go through QJSEngine::evaluate, which parses its input
   * as a script: a top-level `export` is a syntax error there. They must be
   * imported, which is why loading one is a function of its own.
   *
   * The module's `initialize()` is called if it has one, and the module is
   * left on the global object under its base name so that whatever runs next
   * -- another --script, the console, a menu action -- can call into it.
   *
   * Returns the module, or the error value the caller should report: either
   * the import failing, or `initialize()` throwing.
   */
  static QJSValue importModule(QQmlEngine& engine, const QString& path);

  // Used for processing whatever comes from the console
  QQmlEngine m_consoleEngine;

  // Used for instantiating JS::Script* to verify that the script is valid
  // before updating, as well as for running JS UI scripts.
  QQmlEngine m_scriptProcessUIEngine;
  QQmlComponent* m_comp{};
  QQuickWindow* m_window{};

  std::atomic_bool m_processMessages{};
  std::thread m_asioThread;
  ossia::net::network_context_ptr m_asioContext;

  //! One --script argument.
  struct StartScript
  {
    QString source; //!< inline source or file contents; empty for a module
    QString name;   //!< path as given, for diagnostics; empty when inline
    QString file;   //!< canonical path, what importModule() is given
    QString dir;    //!< the script's folder, added as a QML import path
    bool module{};  //!< load with importModule() rather than evaluate()
  };

  //! --script may be repeated; entries run in order. That is what lets a run
  //! load a module and then drive it: `--script mod.mjs --script "mod.go()"`.
  std::vector<StartScript> m_start_scripts;
  bool m_start_script_failed{};
};
}
