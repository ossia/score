#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
Q_IMPORT_PLUGIN(iscore_plugin_scenario)
Q_IMPORT_PLUGIN(iscore_plugin_inspector)
Q_IMPORT_PLUGIN(iscore_plugin_deviceexplorer)
Q_IMPORT_PLUGIN(iscore_plugin_pluginsettings)  // static plug-ins should not be displayed.
Q_IMPORT_PLUGIN(iscore_plugin_curve)

#ifdef ISCORE_NETWORK
Q_IMPORT_PLUGIN(iscore_plugin_network)
#endif

#ifdef ISCORE_COHESION
Q_IMPORT_PLUGIN(iscore_plugin_cohesion)
#endif

#ifdef ISCORE_OSSIA
Q_IMPORT_PLUGIN(iscore_plugin_ossia)
#endif
#endif

int main(int argc, char** argv)
{
#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore_pixmaps);
    Q_INIT_RESOURCE(scenario_resources);
    Q_INIT_RESOURCE(DeviceExplorer);
#endif
    iscore::Application app(argc, argv);
    return app.exec();
}
