#include "mainwindow.hpp"
#include <QApplication>
#include "engine.hpp"

int main(int argc, char *argv[])
{
  QApplication a(argc, argv);
  MainWindow w;
  w.show();

  Engine();

  return a.exec();
}
