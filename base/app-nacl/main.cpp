// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <QApplication>
#include <QtGui>
#include <QtWidgets>
#include <qnamespace.h>
#include <score_static_plugins.hpp>

#if defined(SCORE_OPENGL)
#  include <QSurfaceFormat>
#endif

static void init_plugins()
{
  Q_INIT_RESOURCE(score);
  Q_INIT_RESOURCE(ScenarioResources);
  Q_INIT_RESOURCE(DeviceExplorer);
#if defined(SCORE_PLUGIN_TEMPORALAUTOMATAS)
  Q_INIT_RESOURCE(TAResources);
#endif
}

score::Application* app;

void app_init(int argc, char** argv)
{
  init_plugins();
  app = new score::Application{argc, argv};
  app->init();
}

void app_exit()
{
  delete app;
}

Q_WIDGETS_MAIN(app_init, app_exit);
