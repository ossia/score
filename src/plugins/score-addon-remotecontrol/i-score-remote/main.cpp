#include "RemoteApplication.hpp"

#include <QApplication>

int main(int argc, char** argv)
{
  RemoteApplication app(argc, argv);

  return app.exec();
}
