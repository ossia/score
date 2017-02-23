#include "TestObject.hpp"

#include "TestApplication.hpp"
#include <clocale>
#include <QLocale>
#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif


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
    QLocale::setDefault(QLocale::C);
    std::setlocale(LC_ALL, "C");
    init_plugins();
    TestApplication app(argc, argv);

    TestObject obj{app.context()};

    return app.exec();
}
