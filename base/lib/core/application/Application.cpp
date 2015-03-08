#include <core/application/Application.hpp>

#include <core/model/Model.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <iscore/tools/utilsCPP11.hpp>
#include <iscore/tools/ObjectIdentifier.hpp>
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
    m_model = new Model{this};
    m_view = new View{this};
    m_presenter = new Presenter{m_model, m_view, this};

    // Plugins
    m_pluginManager.reloadPlugins();
    loadPluginData();

    // View
    m_view->show();

    m_presenter->newDocument(m_pluginManager.m_documentPanelList.front());
}

Application::~Application()
{
    this->setParent(nullptr);
    m_app->deleteLater();
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

    for(auto& pnl : m_pluginManager.m_panelList)
    {
        m_presenter->registerPanel(pnl);
    }

    for(auto& pnl : m_pluginManager.m_documentPanelList)
    {
        m_presenter->registerDocumentPanel(pnl);
    }
}

