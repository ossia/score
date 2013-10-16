#include "mainwindow.hpp"
#include <QApplication>
#include "engine.hpp"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName("i-score");
  app.setOrganizationName("OSSIA");
 /// @todo set qrc app.setWindowIcon(QIcon(":/icon.png"));

  MainWindow window;
  window.show();

  Engine();

  return app.exec();
}
