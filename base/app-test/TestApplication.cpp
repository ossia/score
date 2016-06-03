#include "TestApplication.hpp"
#include <core/application/SafeQApplication.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/settings/Settings.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>

#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
TestApplication::TestApplication(int &argc, char **argv):
    NamedObject{"toto", nullptr}
{
    m_app = new SafeQApplication{argc, argv};
    m_instance = this;
    this->setParent(m_app);

    // Settings
    m_settings = std::make_unique<iscore::Settings> (this);

    // MVP
    m_view = new iscore::View{nullptr};
    m_presenter = new iscore::Presenter{m_applicationSettings, *m_settings, m_view, this};
    auto& ctx = m_presenter->applicationContext();

    // Plugins
    iscore::ApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                *m_view,
                m_presenter->menuManager(),
                m_presenter->toolbarManager(),
                m_presenter->actionManager()};

    registrar.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
    auto panels = std::make_unique<iscore::PanelDelegateFactoryList>();
    panels->insert(std::make_unique<iscore::UndoPanelDelegateFactory>());
    registrar.registerFactory(std::move(panels));
    registrar.registerFactory(std::make_unique<iscore::DocumentPluginFactoryList>());
    registrar.registerFactory(std::make_unique<iscore::SettingsDelegateFactoryList>());

    registrar.registerApplicationContextPlugin(new iscore::CoreApplicationPlugin{ctx, *m_presenter});
    registrar.registerApplicationContextPlugin(new iscore::UndoApplicationPlugin{ctx});

    iscore::PluginLoader::loadPlugins(registrar, ctx);
    // Load the settings
    for(auto& elt : ctx.components.factory<iscore::SettingsDelegateFactoryList>())
    {
        m_settings->setupSettingsPlugin(ctx, elt);
    }

    for(iscore::GUIApplicationContextPlugin* app_plug : ctx.components.applicationPlugins())
    {
        app_plug->initialize();
    }

    for(auto& panel_fac : context().components.factory<iscore::PanelDelegateFactoryList>())
    {
        registrar.registerPanel(panel_fac);
    }

    m_view->show();
}

TestApplication::~TestApplication()
{

}

const iscore::ApplicationContext &TestApplication::context() const
{
    return m_presenter->applicationContext();
}

int TestApplication::exec()
{ return m_app->exec(); }
