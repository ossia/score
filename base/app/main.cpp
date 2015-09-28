#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
#include "iscore_static_plugins.hpp"
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
