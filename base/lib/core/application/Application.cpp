#include <core/application/Application.hpp>

#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/undo/UndoControl.hpp>

using namespace iscore;



Application::Application(int& argc, char** argv) :
    NamedObject {"Application", nullptr}
{
    // Application
    // Crashes if put in member initialization list... :(
    m_app = new QApplication(argc, argv);
    this->setParent(m_app);
    this->setObjectName("Application");

    QCoreApplication::setOrganizationName("OSSIA");
    QCoreApplication::setOrganizationDomain("i-score.com");
    QCoreApplication::setApplicationName("i-score");

    qRegisterMetaType<ObjectIdentifierVector> ("ObjectIdentifierVector");

    // Settings
    m_settings = std::make_unique<Settings> (this);

    // MVP
    m_view = new View{this};
    m_presenter = new Presenter{m_view, this};

    // Plugins
    m_pluginManager.reloadPlugins();
    m_pluginManager.addControl(new UndoControl{m_presenter});
    m_pluginManager.addPanel(new UndoPanelFactory);

    loadPluginData();

    // View
    m_view->show();

    if(!m_pluginManager.m_documentPanelList.empty())
        m_presenter->newDocument(m_pluginManager.m_documentPanelList.front());
}

Application::~Application()
{
    this->setParent(nullptr);
    delete m_presenter;
    delete m_app;//m_app->deleteLater();
}

void Application::loadPluginData()
{
    for(auto& set : m_pluginManager.m_settingsList)
    {
        m_settings->setupSettingsPlugin(set);
    }

    for(auto& cmd : m_pluginManager.m_commandList)
    {
        m_presenter->registerPluginControl(cmd);
    }

    std::sort(m_presenter->toolbars().begin(), m_presenter->toolbars().end());
    for(auto& toolbar : m_presenter->toolbars())
    {
        m_view->addToolBar(toolbar.bar);
    }

    for(auto& pnl : m_pluginManager.m_panelList)
    {
        m_presenter->registerPanel(pnl);
    }

    for(auto& pnl : m_pluginManager.m_documentPanelList)
    {
        m_presenter->registerDocumentDelegate(pnl);
    }
}

