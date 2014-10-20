#include <core/application/Application.hpp>
#include <QDebug>

#include <plugin_interface/CustomCommandFactoryPluginInterface.hpp>
#include <plugin_interface/AutoconnectFactoryPluginInterface.hpp>
#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>

#include <interface/autoconnect/Autoconnect.hpp>
using namespace iscore;

Application::Application(int argc, char** argv):
	QObject{}
{
	// Crashes if put in member initialization list... :(
	m_app = std::make_unique<QApplication>(argc, argv);
	
	m_settings = std::make_unique<Settings>(this);
	
	m_model = new Model{this};
	m_view = new View(qobject_cast<QObject*>(this));
	m_presenter = new Presenter(m_model, m_view, this);

	QCoreApplication::setOrganizationName("OSSIA");
	QCoreApplication::setOrganizationDomain("i-score.com");
	QCoreApplication::setApplicationName("i-score");

	connect(&m_pluginManager, &PluginManager::newPlugin,
			this,			  &Application::dispatchPlugin);

	m_pluginManager.reloadPlugins();

	// m_view takes ownership of the widget, this is why
	// we can't use a unique_ptr...
	m_view->show();
}

void Application::dispatchPlugin(QObject* plugin)
{
	qDebug() << plugin->objectName() << "was dispatched";
	auto autoconn_plugin = qobject_cast<AutoconnectFactoryPluginInterface*>(plugin);
	auto menu_plugin = qobject_cast<CustomCommandFactoryPluginInterface*>(plugin);
	auto settings_plugin = qobject_cast<SettingsFactoryPluginInterface*>(plugin);
	auto process_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin);

	if(autoconn_plugin)
	{
		qDebug() << "The plugin has auto-connections";
		// I auto-connect stuff
		for(const auto& connection : autoconn_plugin->autoconnect_list())
		{
			m_autoconnections.push_back(connection);
		}

		doConnections();
	}

	if(menu_plugin)
	{
		qDebug() << "The plugin adds menu options";
		for(const auto& cmd : menu_plugin->customCommand_list())
		{
			m_presenter->addCustomCommand(menu_plugin->customCommand_make(cmd));
		}
	}

	if(settings_plugin)
	{
		qDebug() << "The plugin has settings";
		m_settings->addSettingsPlugin(settings_plugin->settings_make());
	}

	if(process_plugin)
	{
		// Ajouter à la liste des process disponibles
		qDebug() << "The plugin has custom processes";

		auto custom_process = process_plugin->process_make(process_plugin->process_list().first());

		pm = custom_process->makeModel();
		pm->setParent(this);
	}

	doConnections();
}

void Application::doConnections()
{
	for(auto& a : m_autoconnections)
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
