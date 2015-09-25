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

//#define ELPP_STACKTRACE_ON_CRASH
//#include <easylogging++.h>
//INITIALIZE_EASYLOGGINGPP
#include <QSurfaceFormat>
int main(int argc, char** argv)
{
//    START_EASYLOGGINGPP(argc, argv);

#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(DeviceExplorer);
#endif

    QSurfaceFormat fmt;
    //fmt.setMajorVersion(4);
    //fmt.setMinorVersion(0);
    //fmt.setRenderableType(QSurfaceFormat::OpenGL);
    //fmt.setSwapBehavior(QSurfaceFormat::DefaultSwapBehavior);
    fmt.setSamples(2);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    iscore::Application app(argc, argv);
    return app.exec();
}
