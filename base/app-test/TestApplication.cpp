// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TestApplication.hpp"
#include <core/application/SafeQApplication.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/settings/Settings.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/selection/Selection.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/document/DocumentModel.hpp>
TestApplication::TestApplication(int &argc, char **argv):
    QObject{nullptr}
{
    m_app = new SafeQApplication{argc, argv};
    m_instance = this;
    this->setParent(m_app);
    // Settings
    m_settings = std::make_unique<score::Settings> ();

    // MVP
    m_view = new score::View{nullptr};
    m_presenter = new score::Presenter{m_applicationSettings, *m_settings, m_view, this};
    auto& ctx = m_presenter->applicationContext();

    // Plugins
    score::GUIApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                m_presenter->menuManager(),
                m_presenter->toolbarManager(),
                m_presenter->actionManager()};

    GUIApplicationInterface::loadPluginData(ctx, registrar, *m_settings, *m_presenter);

    m_view->show();
}

TestApplication::~TestApplication()
{
    this->setParent(nullptr);
    delete m_view;
    delete m_presenter;

    QApplication::processEvents();
    delete m_app;
}

const score::GUIApplicationContext& TestApplication::context() const
{
    return m_presenter->applicationContext();
}

int TestApplication::exec()
{ return m_app->exec(); }
