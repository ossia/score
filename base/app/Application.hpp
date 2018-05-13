#pragma once
#include <QApplication>
#include <wobjectdefs.h>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>
#include <memory>
#include <score/application/ApplicationContext.hpp>

namespace score
{
class Settings;
} // namespace score

class SafeQApplication;
namespace score
{
class Presenter;
class View;
}

/**
 * @brief Application
 *
 * This class is the main object in score. It is the
 * parent of every other object created.
 * It does instantiate the rest of the software (MVP, settings, plugins).
 */
class Application final
    : public QObject
    , public score::GUIApplicationInterface
{
  W_OBJECT(Application)
  friend class ChildEventFilter;

public:
  Application(int& argc, char** argv);

  Application(
      const score::ApplicationSettings& appSettings, int& argc, char** argv);

  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  ~Application();

  int exec();

  const score::Settings& settings() const
  {
    return m_settings;
  }

  const score::GUIApplicationContext& context() const override;
  const score::ApplicationComponents& components() const override;
  void init(); // m_applicationSettings has to be set.

private:
  void initDocuments();
  void loadPluginData();

  // Base stuff.
  QCoreApplication* m_app;
  score::Settings m_settings;               // Global settings
  score::ProjectSettings m_projectSettings; // Per project

  // MVP
  score::View* m_view{};
  score::Presenter* m_presenter{};

  score::ApplicationSettings m_applicationSettings;
};
