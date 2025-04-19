#include <Vst/Loader.hpp>

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

#include <iostream>
#include <set>

// clang-format off
#include <score/tools/InvisibleWindow.hpp>
#undef OK
#undef NO
#undef Status
// clang-format on
intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
  intptr_t result = 0;

  switch(opcode)
  {
    case audioMasterGetTime: {
      static VstTimeInfo time;
      time.samplePos = 0.;
      time.sampleRate = 44100.;
      time.nanoSeconds = 0.;
      time.ppqPos = 0.;
      time.tempo = 120.;
      time.barStartPos = 0.;
      time.cycleStartPos = 0.;
      time.cycleEndPos = 0.;
      time.timeSigNumerator = 4;
      time.timeSigDenominator = 4;
      time.smpteOffset = 0;
      time.smpteFrameRate = 0;
      time.samplesToNextClock = 512;
      time.flags = kVstNanosValid | kVstPpqPosValid | kVstTempoValid | kVstBarsValid
                   | kVstTimeSigValid | kVstClockValid;
      result = reinterpret_cast<intptr_t>(&time);
      break;
    }
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
    case audioMasterCanDo: {
      static const std::set<std::string_view> supported{
          HostCanDos::canDoSendVstEvents,
          HostCanDos::canDoSendVstMidiEvent,
          HostCanDos::canDoSendVstTimeInfo,
          HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
          HostCanDos::canDoSizeWindow,
          HostCanDos::canDoHasCockosViewAsConfig};
      if(supported.find(static_cast<const char*>(ptr)) != supported.end())
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

    vst::Module plugin{path.toStdString()};

    if(auto m = plugin.getMain())
    {
      if(auto p = (AEffect*)m(vst_host_callback))
      {
        QJsonObject obj;
        obj["UniqueID"] = p->uniqueID;
        obj["Controls"] = p->numParams;
        obj["Author"] = getString(p, effGetVendorString, 0);
        obj["PrettyName"] = getString(p, effGetProductString, 0);
        obj["Version"] = getString(p, effGetVendorVersion, 0);
        obj["Synth"] = bool(p->flags & effFlagsIsSynth);
        obj["Path"] = path;
        obj["Request"] = id;

        p->dispatcher(p, AEffectOpcodes::effClose, 0, 0, nullptr, 0.f);
        return QJsonDocument{obj}.toJson();
      }
    }
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

    socket.open(QUrl("ws://127.0.0.1:37587"));
    app.exec();
  }
  return 1;
}
