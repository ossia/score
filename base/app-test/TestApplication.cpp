#include "TestApplication.hpp"
#include <core/application/SafeQApplication.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/settings/Settings.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>

#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

TestApplication::TestApplication(int &argc, char **argv):
    NamedObject{"toto", nullptr}
{
    m_app = new SafeQApplication{argc, argv};
    m_instance = this;
    this->setParent(m_app);

    // Settings
    m_settings = std::make_unique<iscore::Settings> (nullptr);

    // MVP
    m_view = new iscore::View{nullptr};
    m_presenter = new iscore::Presenter{m_applicationSettings, m_view, this};
    auto& ctx = m_presenter->applicationContext();

    // Plugins
    iscore::ApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                *m_view,
                *m_settings,
                m_presenter->menuBar(),
                m_presenter->toolbars(),
                m_presenter};

    registrar.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
    registrar.registerFactory(std::make_unique<iscore::DocumentPluginFactoryList>());

    iscore::PluginLoader::loadPlugins(registrar, ctx);

    registrar.registerApplicationContextPlugin(new iscore::UndoApplicationPlugin{ctx, m_presenter});
    registrar.registerPanel(new UndoPanelFactory);

    std::sort(m_presenter->toolbars().begin(), m_presenter->toolbars().end());
    for(auto& toolbar : m_presenter->toolbars())
    {
        m_view->addToolBar(toolbar.bar);
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
