#include <iostream>
#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(ScenarioPlugin)
Q_IMPORT_PLUGIN(InspectorPlugin)
Q_IMPORT_PLUGIN(DeviceExplorerPlugin)
Q_IMPORT_PLUGIN(PluginSettingsPlugin) // static plug-ins should not be displayed.
Q_IMPORT_PLUGIN(NetworkPlugin)
#endif

int main(int argc, char **argv)
{
	iscore::Application app(argc, argv);
	return app.exec();
}
