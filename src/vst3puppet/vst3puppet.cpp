#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>

#include <iostream>

#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>

// clang-format off
#include <score/tools/InvisibleWindow.hpp>
#undef OK
#undef NO
#undef Status
using namespace Steinberg;
QString load_vst(const QString& path, int id)
{
  try
  {
    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if(!isFile)
    {
      std::cerr << "Invalid path: " << path.toStdString() << std::endl;
      return {};
    }

    std::string err;
    auto module = VST3::Hosting::Module::create(path.toStdString(), err);

    if(!module)
    {
      std::cerr << "Failed to load VST3 " << path.toStdString() << err << std::endl;
    }

    QJsonObject root;

    QJsonArray arr;
    const auto& fac = module->getFactory();
    const auto& fi = fac.info();
    for(const auto& cls : fac.classInfos())
    {
      if(cls.category() == kVstAudioEffectClass)
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
    root["Url"] = QString::fromStdString(fi.url());
    root["Email"] = QString::fromStdString(fi.email());
    root["Path"] = path;
    root["Request"] = id;
    root["Classes"] = arr;

    return QJsonDocument{root}.toJson();
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return {};
}

int main(int argc, char** argv)
{
  if(argc > 1)
  {
    int id = 0;
    if(argc > 2)
    {
      id = QString(argv[2]).toInt();
    }
    QCoreApplication app(argc, argv);
    invisible_window w;

    QWebSocket socket;

    bool socket_ready{}, vst_ready{};
    QString json_ret;

    auto onReady = [&] {
      if(socket_ready && vst_ready)
      {
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

    QObject::connect(&socket, &QWebSocket::connected, &app, [&] {
      socket_ready = true;
      onReady();
    });

    QObject::connect(
        &socket,
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
#else
        &QWebSocket::errorOccurred,
#endif
        &app, [&] {
          qDebug() << socket.errorString();
          app.exit(1);
        });
    QObject::connect(&socket, &QWebSocket::disconnected, &app, [&] {
      qDebug() << socket.errorString();
      app.exit(1);
    });

    QTimer::singleShot(10000, [&] {
      qDebug() << "timeout";
      qApp->exit(1);
    });

    socket.open(QUrl("ws://127.0.0.1:37588"));
    app.exec();
  }
  return 1;
}
