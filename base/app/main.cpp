#include <core/application/Application.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
#include <iscore_static_plugins.hpp>
#endif

#include <QSurfaceFormat>

int main(int argc, char** argv)
{
#if defined(ISCORE_STATIC_PLUGINS)
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(AutomationResources);
    Q_INIT_RESOURCE(MappingResources);
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
