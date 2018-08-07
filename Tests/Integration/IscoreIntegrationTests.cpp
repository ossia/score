#include "IscoreIntegrationTests.hpp"

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/Window.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/selection/Selection.hpp>

#include <core/presenter/Presenter.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <score/document/DocumentInterface.hpp>

score::ApplicationComponentsData&TestBase::componentsData()
{

  return m_presenter->components();
}

TestBase::TestBase(int& argc, char** argv)
{
  qApp->setAttribute(Qt::AA_Use96Dpi, true);
  QTEST_DISABLE_KEYPAD_NAVIGATION;
  QTEST_ADD_GPU_BLACKLIST_SUPPORT;

  m_applicationSettings.gui = false;
  m_app = new QCoreApplication{argc, argv};
  m_instance = this;
  this->setParent(m_app);
  // Settings

  // MVP
  m_presenter
      = new score::Presenter{m_applicationSettings, m_settings, m_projectSettings, nullptr, this};
  auto& ctx = m_presenter->applicationContext();

  // Plugins
  score::GUIApplicationRegistrar registrar{
    m_presenter->components(), ctx, m_presenter->menuManager(),
        m_presenter->toolbarManager(), m_presenter->actionManager()};

  GUIApplicationInterface::loadPluginData(
        ctx, registrar, m_settings, *m_presenter);
}

const score::GUIApplicationContext&TestBase::context() const
{
  return m_presenter->applicationContext();
}

const score::ApplicationComponents&TestBase::components() const
{
  return m_presenter->applicationComponents();
}

int TestBase::exec()
{
  return m_app->exec();
}

TestBase::~TestBase()
{
  this->setParent(nullptr);
  delete m_presenter;

  QApplication::processEvents();
  delete m_app;
}
