// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Models/NodeModel.hpp>
#include <Models/WidgetListModel.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <RemoteApplication.hpp>

int main(int argc, char* argv[])
{
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  qmlRegisterType<RemoteUI::WidgetListData>();
  qmlRegisterType<RemoteUI::NodeModel>();

  RemoteUI::RemoteApplication app{argc, argv};

  return app.exec();
}
