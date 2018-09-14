#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>

namespace score
{
class Settings;
class View;
class Presenter;
} // namespace score

class QApplication;

class TestApplication final : public QObject,
                              public score::GUIApplicationInterface
{
public:
  TestApplication(int& argc, char** argv);
  ~TestApplication();

  const score::GUIApplicationContext& context() const override;
  const score::ApplicationComponents& components() const override
  {
    return context().components;
  }

  int exec();

  // Base stuff.
  QApplication* m_app;
  std::unique_ptr<score::Settings> m_settings; // Global settings

  // MVP
  score::View* m_view{};
  score::Presenter* m_presenter{};

  score::ApplicationSettings m_applicationSettings;
};
