#include <core/application/Application.hpp>

#include <core/model/Model.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>
#include <core/application/ChildEventFilter.hpp>
#include <core/tools/utilsCPP11.hpp>
#include <core/tools/ObjectIdentifier.hpp>
using namespace iscore;



Application::Application (int& argc, char** argv) :
    NamedObject {"Application", nullptr}
{
    // Application
    // Crashes if put in member initialization list... :(
    m_app = new QApplication (argc, argv);
    this->setParent (m_app);
    this->setObjectName ("Application");

    m_app->installEventFilter (new ChildEventFilter (this) );

    QCoreApplication::setOrganizationName ("OSSIA");
    QCoreApplication::setOrganizationDomain ("i-score.com");
    QCoreApplication::setApplicationName ("i-score");

    qRegisterMetaType<ObjectIdentifierVector> ("ObjectIdentifierVector");

    // Settings
    m_settings = std::make_unique<Settings> (this);

    // MVP
    m_model = new Model{this};

    m_view = new View (qobject_cast<QObject*> (this) );
    m_presenter = new Presenter (m_model, m_view, this);

    // Plugins
    m_pluginManager.reloadPlugins();
    loadPluginData();

    m_presenter->newDocument();

    // View
    m_view->show();
}

Application::~Application()
{
    this->setParent (nullptr);

    m_app->deleteLater();
}

void Application::loadPluginData()
{
    for (auto& set : m_pluginManager.m_settingsList)
    {
        m_settings->setupSettingsPlugin (set);
    }

    for (auto& cmd : m_pluginManager.m_commandList)
    {
        m_presenter->registerPluginControl (cmd);
    }

    for (auto& pnl : m_pluginManager.m_panelList)
    {
        m_presenter->registerPanel (pnl);
    }

    if (!m_pluginManager.m_documentPanelList.empty() )
    {
        m_presenter->setDocumentPanel (m_pluginManager.m_documentPanelList.front() );
    }
}

void Application::doConnections()
{
    for (auto& a : m_pluginManager.m_autoconnections)
    {
        auto potential_sources = a.getMatchingChildrenForSource (this) + a.getMatchingChildrenForSource (view() );
        auto potential_targets = a.getMatchingChildrenForTarget (this) + a.getMatchingChildrenForTarget (view() );

        for (auto& s_elt : potential_sources)
        {
            for (auto& t_elt : potential_targets)
            {
                //s_elt->disconnect(a.source.method, t_elt);
                /*bool res = */t_elt->connect (s_elt,
                                               a.source.method,
                                               a.target.method,
                                               (Qt::ConnectionType) (Qt::QueuedConnection | Qt::UniqueConnection) );
            }
        }
    }
}

// TODO essayer de voir si on peut réduire la duplication de code ici ?
void Application::doConnections (QObject* obj)
{
    // 1. Chercher s'il est source
    for (auto& a : m_pluginManager.m_autoconnections)
    {
        // obj is source
        if ( (a.source.type == Autoconnect::ObjectRepresentationType::QObjectName &&
                obj->objectName() == QString (a.source.name) ) ||
                (a.source.type == Autoconnect::ObjectRepresentationType::Inheritance &&
                 obj->inherits (a.source.name) ) )
        {
            auto potential_targets = a.getMatchingChildrenForTarget (this, view() );

            for (auto& t_elt : potential_targets)
            {
                /*bool res = */t_elt->connect (obj,
                                               a.source.method,
                                               a.target.method,
                                               (Qt::ConnectionType) (Qt::QueuedConnection | Qt::UniqueConnection) );
            }
        }

        // obj is target
        if ( (a.target.type == Autoconnect::ObjectRepresentationType::QObjectName &&
                obj->objectName() == QString (a.target.name) )
                ||
                (a.target.type == Autoconnect::ObjectRepresentationType::Inheritance &&
                 obj->inherits (a.target.name) ) )
        {
            // Obj. est source.
            auto potential_sources = a.getMatchingChildrenForSource (this, view() );

            for (auto& s_elt : potential_sources)
            {
                /*bool res = */obj->connect (s_elt,
                                             a.source.method,
                                             a.target.method,
                                             Qt::UniqueConnection);
            }
        }
    }
}

void Application::addAutoconnection (Autoconnect a)
{
    m_pluginManager.m_autoconnections.push_back (a);
    doConnections();
}

