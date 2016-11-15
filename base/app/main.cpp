#include "Application.hpp"
#include <QApplication>
#include <QPixmapCache>
#include <qnamespace.h>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif

#if defined(ISCORE_STATIC_QT)
  #if defined(__linux__)
    #include <QtPlugin>

    Q_IMPORT_PLUGIN (QXcbIntegrationPlugin)
  #endif
#endif


#include <QSurfaceFormat>

static void init_plugins()
{
// TODO generate this too
#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(DeviceExplorer);
#if defined(ISCORE_PLUGIN_TEMPORALAUTOMATAS)
    Q_INIT_RESOURCE(TAResources);
#endif
#endif
}

int main(int argc, char** argv)
{
    init_plugins();

#if defined(ISCORE_OPENGL)
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setMajorVersion(4);
    fmt.setMinorVersion(1);
    fmt.setSamples(2);
    fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    fmt.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(fmt);
#endif

    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QPixmapCache::setCacheLimit(819200);
    Application app(argc, argv);
    app.init();
    return app.exec();
}
