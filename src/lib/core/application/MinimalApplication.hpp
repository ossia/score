#pragma once

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/view/Window.hpp>

#include <QApplication>

#include <clocale>
#include <memory>

#if defined(SCORE_STATIC_PLUGINS)
#include <score_static_plugins.hpp>
#endif

namespace score
{
class MinimalApplication final
    : public QObject
    , public score::GUIApplicationInterface
{
public:
  // Static so they are alive before construction (the delegating constructor
  // below reads them) and outlive the QApplication, which keeps a reference to
  // argc for its whole lifetime.
  static inline int default_argc = 1;
  static inline const char* default_argv[2] = {"score", nullptr};

  MinimalApplication()
      : MinimalApplication{default_argc, (char**)default_argv}
  {
  }

  MinimalApplication(int& argc, char** argv)
      : QObject{nullptr}
      , m_app{new QApplication{argc, argv}}
  {
#if defined(SCORE_STATIC_PLUGINS)
    score_init_static_plugins();
#endif

    m_instance = this;
    this->setParent(m_app.get());

    m_presenter
        = new score::Presenter{m_applicationSettings, m_settings, m_pset, nullptr, this};

    GUIApplicationInterface::loadPluginData(m_settings, *m_presenter);
  }

  ~MinimalApplication() override
  {
    this->setParent(nullptr);
    // Drain the event queue before deleting the presenter: deferred slots
    // queued during plugin load (e.g. Scenario::SearchWidget's deferred init
    // -> findDeviceExplorerWidgetInstance) read the presenter-owned application
    // context, so dispatching them after delete m_presenter is a use-after-free.
    QApplication::processEvents();
    delete m_presenter;
    // m_app (the QApplication) is destroyed last, after the score::Settings /
    // ProjectSettings members below: their models are parented to the
    // QApplication, so destroying it first would double-free them.
  }

  const score::GUIApplicationContext& context() const override
  {
    return m_presenter->applicationContext();
  }

  const score::ApplicationComponents& components() const override
  {
    return context().components;
  }

  score::ApplicationComponentsData& componentsData()
  {
    return m_presenter->components();
  }

  int exec() { return m_app->exec(); }

  // Declared first so it is destroyed last (after the settings members, whose
  // models are parented to it).
  std::unique_ptr<QApplication> m_app;
  score::Settings m_settings;
  score::ProjectSettings m_pset;
  score::Presenter* m_presenter{};
  score::ApplicationSettings m_applicationSettings;
};

class MinimalGUIApplication final
    : public QObject
    , public score::GUIApplicationInterface
{
public:
  MinimalGUIApplication(int& argc, char** argv, bool show = true)
      : QObject{nullptr}
      , m_app{new QApplication{argc, argv}}
  {
    m_show = show;
#if defined(SCORE_STATIC_PLUGINS)
    score_init_static_plugins();
#endif

    m_instance = this;
    this->setParent(m_app.get());

    m_view = new score::View{nullptr};
    m_presenter
        = new score::Presenter{m_applicationSettings, m_settings, m_pset, m_view, this};

    GUIApplicationInterface::loadPluginData(m_settings, *m_presenter);

    if(m_show)
      m_view->show();
  }

  ~MinimalGUIApplication() override
  {
    this->setParent(nullptr);
    // See ~MinimalApplication: drain queued slots while the presenter is still
    // alive, since deferred plugin-load slots read the presenter-owned context.
    QApplication::processEvents();
    delete m_presenter;
    // m_app (the QApplication) is destroyed last (declared first), after the
    // score::Settings members whose models are parented to it.
  }

  const score::GUIApplicationContext& context() const override
  {
    return m_presenter->applicationContext();
  }

  const score::ApplicationComponents& components() const override
  {
    return context().components;
  }

  score::ApplicationComponentsData& componentsData()
  {
    return m_presenter->components();
  }

  score::View& view() const { return *m_view; }

  int exec() { return m_app->exec(); }

  // Whether to show() the main window. Tests that only need a valid document
  // presenter (not actual painting) can leave it hidden.
  bool m_show{true};

  // Declared first so it is destroyed last (after the settings members, whose
  // models are parented to it).
  std::unique_ptr<QApplication> m_app;
  score::Settings m_settings;
  score::ProjectSettings m_pset;
  score::View* m_view{};
  score::Presenter* m_presenter{};
  score::ApplicationSettings m_applicationSettings;
};
}
