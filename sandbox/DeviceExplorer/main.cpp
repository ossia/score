
#include <QApplication>
#include "MainWindow.hpp"


int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  MainWindow window;
  window.show();

  if (argc > 1) {
    QString filename = QString::fromStdString(argv[1]);
    window.load(filename);
  }

  return app.exec();
}
