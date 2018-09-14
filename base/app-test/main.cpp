// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TestApplication.hpp"
#include "TestObject.hpp"

#include <QLocale>

#include <clocale>
static void init_plugins()
{
// TODO generate this too
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(score);
  Q_INIT_RESOURCE(ScenarioResources);
  Q_INIT_RESOURCE(DeviceExplorer);
#if defined(SCORE_PLUGIN_TEMPORALAUTOMATAS)
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
