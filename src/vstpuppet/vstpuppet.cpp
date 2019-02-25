#include <Media/Effect/VST/VSTLoader.hpp>

#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QTimer>
#include <QWebSocket>
#include <iostream>
#include <set>

intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr,
    float opt)
{
  intptr_t result = 0;

  switch (opcode)
  {
    case audioMasterGetTime:
      result = 0;
      break;
    case audioMasterSizeWindow:
      result = 1;
      break;
    case audioMasterNeedIdle:
      break;
    case audioMasterIdle:
      break;
    case audioMasterCurrentId:
      result = effect->uniqueID;
      break;
    case audioMasterUpdateDisplay:
      break;
    case audioMasterAutomate:
      break;
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterGetInputLatency:
      break;
    case audioMasterGetOutputLatency:
      break;
    case audioMasterVersion:
      result = kVstVersion;
      break;
    case audioMasterGetSampleRate:
      result = 44100;
      break;
    case audioMasterGetBlockSize:
      result = 512;
      break;
    case audioMasterGetCurrentProcessLevel:
      result = kVstProcessLevelUser;
      break;
    case audioMasterGetAutomationState:
      result = kVstAutomationUnsupported;
      break;
    case audioMasterGetLanguage:
      result = kVstLangEnglish;
      break;
    case audioMasterGetVendorVersion:
      result = 1;
      break;
    case audioMasterGetVendorString:
      std::copy_n("ossia", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterGetProductString:
      std::copy_n("score", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterBeginEdit:
      break;
    case audioMasterEndEdit:
      break;
    case audioMasterOpenFileSelector:
      break;
    case audioMasterCloseFileSelector:
      break;
    case audioMasterCanDo:
    {
      static const std::set<std::string_view> supported{
          HostCanDos::canDoSendVstEvents,
          HostCanDos::canDoSendVstMidiEvent,
          HostCanDos::canDoSendVstTimeInfo,
          HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
          HostCanDos::canDoSizeWindow,
          HostCanDos::canDoHasCockosViewAsConfig};
      if (supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}

static QString getString(AEffect* fx, AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  fx->dispatcher(fx, op, param, 0, paramName, 0.f);
  return QString::fromUtf8(paramName);
}

QString load_vst(const QString& path)
{
  try
  {
    bool isFile = QFile(QUrl(path).toString(QUrl::PreferLocalFile)).exists();
    if (!isFile)
    {
      std::cerr << "Invalid path: " << path.toStdString() << std::endl;
      return {};
    }

    Media::VST::VSTModule plugin{path.toStdString()};

    if (auto m = plugin.getMain())
    {
      if (auto p = (AEffect*)m(vst_host_callback))
      {
        QJsonObject obj;
        obj["UniqueID"] = p->uniqueID;
        obj["Controls"] = p->numParams;
        obj["Author"] = getString(p, effGetVendorString, 0);
        obj["PrettyName"] = getString(p, effGetProductString, 0);
        obj["Version"] = getString(p, effGetVendorVersion, 0);
        obj["Synth"] = bool(p->flags & effFlagsIsSynth);
        obj["Path"] = path;

        return QJsonDocument{obj}.toJson();
      }
    }
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
    QGuiApplication app(argc, argv);
    QWebSocket socket;

    QObject::connect(&socket, &QWebSocket::connected,
            &app, [&] {
      auto ret = load_vst(argv[1]);
      std::cout << ret.toStdString();
      socket.sendTextMessage(ret);
      socket.flush();
      socket.close();

      app.exit(ret.isEmpty() ? 1 : 0);
    });

    QObject::connect(&socket, qOverload<QAbstractSocket::SocketError>(&QWebSocket::error), &app,
                     [&] (QAbstractSocket::SocketError) { qDebug() << socket.errorString(); app.exit(1); });

    socket.open(QUrl("ws://127.0.0.1:37587"));
    app.exec();
  }
  return 1;
}
