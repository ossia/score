#include "Application.hpp"
#include <QApplication>
#include <qnamespace.h>
#include <QtGui>
#include <QtWidgets>
#include <iscore_static_plugins.hpp>

#if defined(ISCORE_OPENGL)
#include <QSurfaceFormat>
#endif

static void init_plugins()
{
    Q_INIT_RESOURCE(iscore);
    Q_INIT_RESOURCE(ScenarioResources);
    Q_INIT_RESOURCE(AutomationResources);
    Q_INIT_RESOURCE(DeviceExplorer);
#if defined(ISCORE_PLUGIN_MAPPING)
    Q_INIT_RESOURCE(MappingResources);
#endif
#if defined(ISCORE_PLUGIN_TEMPORALAUTOMATAS)
    Q_INIT_RESOURCE(TAResources);
#endif
}

iscore::Application * app;

void app_init(int argc, char **argv)
{
    init_plugins();
    app = new iscore::Application{argc, argv};
    app->init();
}

void app_exit()
{
    delete app;
}

Q_WIDGETS_MAIN(app_init, app_exit);
