#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/settings/Settings.hpp>

#include <QtTest/QTest>

QT_BEGIN_NAMESPACE
QTEST_ADD_GPU_BLACKLIST_SUPPORT_DEFS
QT_END_NAMESPACE

class QCoreApplication;
namespace score
{
class Settings;
class View;
class Presenter;
}

class TestBase
    : public QObject
    , public score::GUIApplicationInterface
{
protected:
  // Base stuff.
  QCoreApplication* m_app;
  score::Settings m_settings; // Global settings
  score::ProjectSettings m_projectSettings; // Per project

  // MVP
  score::Presenter* m_presenter{};

  score::ApplicationSettings m_applicationSettings;
  score::ApplicationComponentsData& componentsData();
public:
  TestBase(int& argc, char** argv);

  const score::GUIApplicationContext& context() const override;
  const score::ApplicationComponents& components() const override;

  int exec();

  ~TestBase();

};

#define SCORE_INTEGRATION_TEST(TestObject) \
int main(int argc, char** argv) \
{ \
    TestObject tc(argc, argv); \
    QTEST_SET_MAIN_SOURCE_PATH \
    return QTest::qExec(&tc, argc, argv); \
}
