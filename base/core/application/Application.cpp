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

void Application::dispatchPlugin(QObject* plugin)
{
	qDebug() << plugin->objectName() << "was dispatched";

	if(auto settings_plugin = qobject_cast<SettingsFactoryPluginInterface*>(plugin))
	{
		qDebug() << "I am a settings";
		m_settings.addSettingsPlugin(settings_plugin->settings_make());
	}

	if(auto process_plugin = qobject_cast<ProcessFactoryPluginInterface*>(plugin))
	{
		qDebug() << "I have custom processes";
		auto custom_process = process_plugin->process_make(process_plugin->process_list().first());
	}
}
