#include <core/application/Application.hpp>
#include <QDebug>

#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>

#include <interface/autoconnect/Autoconnect.hpp>
using namespace iscore;

Application::Application(int argc, char** argv):
	m_app{std::make_unique<QApplication>(argc, argv)},
	m_model{std::make_unique<Model>()},
	m_view{std::make_unique<View>()},
	m_presenter{std::make_unique<Presenter>(m_model.get(), m_view.get())}
{
	QCoreApplication::setOrganizationName("OSSIA");
	QCoreApplication::setOrganizationDomain("i-score.com");
	QCoreApplication::setApplicationName("i-score");

	connect(&m_pluginManager, &PluginManager::newPlugin,
			this, &Application::dispatchPlugin);

	m_pluginManager.reloadPlugins();

	m_view->setCentralWidget(m_settings.view());
	m_view->show();
}

void Application::dispatchPlugin(QObject* plugin)
{
	qDebug() << plugin->objectName() << "was dispatched";
	auto settings_plugin = qobject_cast<SettingsFactoryPluginInterface*>(plugin);
	auto process_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin);

	std::unique_ptr<iscore::Process> custom_process;
	if(settings_plugin)
	{
		qDebug() << "I am a settings";
		m_settings.addSettingsPlugin(settings_plugin->settings_make());
	}

	if(process_plugin)
	{
		qDebug() << "I have custom processes";
		custom_process = process_plugin->process_make(process_plugin->process_list().first());
		pm = custom_process->makeModel();
		pm->setParent(this);
	}

	if(settings_plugin && process_plugin)
	{
		Autoconnect a{{Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldSettingsModel", SIGNAL(textChanged())},
					  {Autoconnect::ObjectRepresentationType::QObjectName, "HelloWorldProcessModel", SLOT(setText())}};
		
		// Find sources
		QList<QObject*> potential_sources = a.getMatchingChildrenForSource(m_settings.model());
		QList<QObject*> potential_targets = a.getMatchingChildrenForTarget(this);
		
		// Find targets (or just do on the "make'd" element ? we have to search if it is on the newly created object)
		for(auto& s_elt : potential_sources)
		{
			for(auto& t_elt : potential_targets)
			{
				qDebug() << "FOUND";
				t_elt->connect(s_elt,
							   a.source.method,
							   a.target.method,
							   Qt::UniqueConnection);
				
			}
		}
		
	}
}
