#include <core/application/Application.hpp>
#include <QApplication>
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

#if defined(ISCORE_OPENGL)
#include <QSurfaceFormat>
#endif

int main(int argc, char** argv)
{
#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(AutomationResources);
#if defined(ISCORE_PLUGIN_MAPPING)
    Q_INIT_RESOURCE(MappingResources);
#endif
    Q_INIT_RESOURCE(DeviceExplorer);
#endif
#if defined(ISCORE_OPENGL)
    QSurfaceFormat fmt;
    fmt.setSamples(2);
    QSurfaceFormat::setDefaultFormat(fmt);
#endif
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    iscore::Application app(argc, argv);
    return app.exec();
}
