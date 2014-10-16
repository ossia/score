#include <core/application/Application.hpp>
#include <QDebug>

#include <plugin_interface/ProcessFactoryPluginInterface.hpp>
#include <plugin_interface/SettingsFactoryPluginInterface.hpp>
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

/*
 * auto-connect : faire à chaque fois qu'on fait un make_qqch sur un truc issu d'un plug-in.
 * Pour que ça marche on utilise la notion parent - children des QObject, qui permettent
 * facilement de faire une recherche récursive.
 */
class Autoconnect
{
	public:
		// CF : http://www.qtforum.org/article/515/connecting-signals-and-slots-by-name-at-runtime.html
		QString origin{"HelloWorldSettingsModel"};
		QString signal{"2textChanged()"};
		QString target{"HelloWorldProcessModel"};
		QString target_method{"1setText()"};
};


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
	}

	if(settings_plugin && process_plugin)
	{
		Autoconnect a;
		for(auto& elt : m_settings.model()->findChildren<QObject*>(a.origin))
		{
			qDebug() << "FOUND";
			pm = custom_process->makeModel();
			if(pm->objectName() == a.target)
			{
				pm->connect(elt,
							a.signal.toLatin1().constData(),
							a.target_method.toLatin1().constData());
			}
		}
	}
}
