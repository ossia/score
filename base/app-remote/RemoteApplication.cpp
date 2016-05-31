#include "RemoteApplication.hpp"
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

RemoteApplication::RemoteApplication(int &argc, char **argv):
    NamedObject{"remote", nullptr},
    m_app{new SafeQApplication{argc, argv}}
{

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
        m_settings->setupSettingsPlugin(elt);
    }

    m_presenter->setupGUI();

    for(iscore::GUIApplicationContextPlugin* app_plug : ctx.components.applicationPlugins())
    {
        app_plug->initialize();
    }

    for(auto& panel_fac : context().components.factory<iscore::PanelDelegateFactoryList>())
    {
        registrar.registerPanel(panel_fac);
    }


    engine.load(QUrl(QStringLiteral("qrc:/resources/main.qml")));

    for(auto obj : engine.rootObjects())
    {
        connect(obj, SIGNAL(itemClicked(int)), &m_triggers, SLOT(on_rowPressed(int)));
        connect(obj, SIGNAL(play()), &m_triggers, SLOT(on_play()));
        connect(obj, SIGNAL(pause()), &m_triggers, SLOT(on_pause()));
        connect(obj, SIGNAL(stop()), &m_triggers, SLOT(on_stop()));
        connect(obj, SIGNAL(addressChanged(QString)), &m_triggers, SLOT(on_addressChanged(QString)));

        obj->setProperty("model", QVariant::fromValue(&m_triggers.m_activeTimeNodes));
    }

}

RemoteApplication::~RemoteApplication()
{

}
const iscore::ApplicationContext &RemoteApplication::context() const
{
    return m_presenter->applicationContext();
}

int RemoteApplication::exec()
{ return m_app->exec(); }
