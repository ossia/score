#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>
#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>

#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QTimer>
#include <QFile>
#include <QWebSocket>
#include <iostream>
#include <set>
#include <QWindow>
using namespace Steinberg;
QString load_vst(const QString& path, int id)
{
  try
  {
    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if (!isFile)
    {
      std::cerr << "Invalid path: " << path.toStdString() << std::endl;
      return {};
    }

    std::string err;
    auto module = VST3::Hosting::Module::create(path.toStdString(), err);

    if (!module)
    {
      std::cerr << "Failed to load VST3 " << path.toStdString() <<  err << std::endl;
    }

    const auto& info = module->getFactory().info();
    QJsonObject root;

    QJsonArray arr;
    for(const auto& cls : module->getFactory().classInfos())
    {
      if (cls.category() == kVstAudioEffectClass)
      {
        QJsonObject obj;

        obj["UID"] = QString::fromStdString(cls.ID().toString());
        obj["Cardinality"] = cls.cardinality();
        obj["Category"] = QString::fromStdString(cls.category());
        obj["Name"] = QString::fromStdString(cls.name());
        obj["Vendor"] = QString::fromStdString(cls.vendor());
        obj["Version"] = QString::fromStdString(cls.version());
        obj["SDKVersion"] = QString::fromStdString(cls.sdkVersion());
        obj["Subcategories"] = QString::fromStdString(cls.subCategoriesString());
        obj["ClassFlags"] = (double)cls.classFlags();

        arr.push_back(obj);
      }
    }
    root["Name"] = QString::fromStdString(module->getName());
    root["Path"] = path;
    root["Request"] = id;
    root["Classes"] = arr;

    return QJsonDocument{root}.toJson();
  }
  catch (const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return {};
}

int main(int argc, char** argv)
{
  if (argc > 1)
  {
    int id = 0;
    if(argc > 2) {
        id = QString(argv[2]).toInt();
    }
    QGuiApplication app(argc, argv);
    QWindow w;
    w.setWidth(1);
    w.setHeight(1);
    w.setFlag(Qt::FramelessWindowHint);
    w.setFlag(Qt::X11BypassWindowManagerHint);
    w.show();

    QWebSocket socket;

    bool socket_ready{}, vst_ready{};
    QString json_ret;

    auto onReady = [&] {
      if(socket_ready && vst_ready) {
        socket.sendTextMessage(json_ret);
        socket.flush();
        socket.close();
        app.exit(json_ret.isEmpty() ? 1 : 0);
      }
    };

    QTimer::singleShot(32, [&] {
      json_ret = load_vst(argv[1], id);
      std::cout << json_ret.toStdString();
      vst_ready = true;
      onReady();
    });

    QObject::connect(&socket, &QWebSocket::connected,
            &app, [&] {
      socket_ready = true;
      onReady();
    });

    QObject::connect(&socket, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), &app,
                     [&] { qDebug() << socket.errorString(); app.exit(1); });
    QObject::connect(&socket, &QWebSocket::disconnected, &app,
                     [&] { qDebug() << socket.errorString(); app.exit(1); });

    QTimer::singleShot(10000, [&] { qDebug() << "timeout"; qApp->exit(1); });

    socket.open(QUrl("ws://127.0.0.1:37588"));
    app.exec();
  }
  return 1;
}
