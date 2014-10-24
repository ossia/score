#include <core/application/Application.hpp>

#include <core/model/Model.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

using namespace iscore;

Application::Application(int& argc, char** argv):
	QObject{}
{
	// Application
	// Crashes if put in member initialization list... :(
	m_app = std::make_unique<QApplication>(argc, argv);
	this->setParent(m_app.get());

	QCoreApplication::setOrganizationName("OSSIA");
	QCoreApplication::setOrganizationDomain("i-score.com");
	QCoreApplication::setApplicationName("i-score");

	// Settings
	m_settings = std::make_unique<Settings>(this);

	m_pluginManager.reloadPlugins();
	loadGlobalPluginData();
	on_New();
}

Application::~Application()
{
	this->setParent(nullptr);
}


// Passe dans le Presenter, qui l'applique au Document
void Application::on_New()
{
	m_app->removePostedEvents(0, 0);
	// MVP
	//if(m_view) m_view->hide();
	//delete m_view;
	delete m_model;
	delete m_presenter;

	m_model = new Model{this}; // ? Utile ? Ce sont les settings, non ?
	if (!m_view)
		m_view = new View(qobject_cast<QObject*>(this));
	m_presenter = new Presenter(m_model, m_view, this);
	m_view->setPresenter(m_presenter);
	// Plugins
	loadPerInstancePluginData();
	doConnections();

	// View
	m_view->show();
}

void Application::loadPerInstancePluginData()
{
	for(auto& cmd : m_pluginManager.m_commandList)
		m_presenter->setupCommand(cmd);

	for(auto& pnl : m_pluginManager.m_panelList)
		m_presenter->addPanel(pnl);
}

void Application::loadGlobalPluginData()
{
	for(auto& set : m_pluginManager.m_settingsList)
		m_settings->setupSettingsPlugin(set);
}

void Application::doConnections()
{
	for(auto& a : m_pluginManager.m_autoconnections)
	{
		auto potential_sources = a.getMatchingChildrenForSource(this);
		auto potential_targets = a.getMatchingChildrenForTarget(this);

		for(auto& s_elt : potential_sources)
		{
			for(auto& t_elt : potential_targets)
			{
				t_elt->connect(s_elt,
							   a.source.method,
							   a.target.method,
							   Qt::UniqueConnection);

			}
		}
	}
}

void Application::childEvent(QChildEvent* ev)
{
	if(ev->type() == QEvent::ChildAdded)
	{
		doConnections();
	}
}

