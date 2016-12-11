#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <RemoteApplication.hpp>
#include <Models/WidgetListModel.hpp>
#include <Models/NodeModel.hpp>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    qmlRegisterType<RemoteUI::WidgetListData>();
    qmlRegisterType<RemoteUI::NodeModel>();

    RemoteUI::RemoteApplication app{argc, argv};

    return app.exec();
}
