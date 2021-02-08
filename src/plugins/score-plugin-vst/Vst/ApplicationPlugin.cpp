#include "ApplicationPlugin.hpp"

#include <Media/Effect/Settings/Model.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Loader.hpp>

#include <QWebSocket>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/audio/audio_protocol.hpp>

#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <Audio/AudioDevice.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(vst::ApplicationPlugin)

// TODO remove me in a few versions
static bool vst_invalid_format = false;
template <>
void DataStreamReader::read<vst::VSTInfo>(const vst::VSTInfo& p)
{
  m_stream << p.path << p.prettyName << p.displayName << p.author << p.uniqueID << p.controls
           << p.isSynth << p.isValid;
}
template <>
void DataStreamWriter::write<vst::VSTInfo>(vst::VSTInfo& p)
{
  if (!vst_invalid_format)
    m_stream >> p.path >> p.prettyName >> p.displayName >> p.author >> p.uniqueID >> p.controls
        >> p.isSynth >> p.isValid;
  if (m_stream.stream.status() != QDataStream::Status::Ok)
  {
    vst_invalid_format = true;
  }
}

Q_DECLARE_METATYPE(vst::VSTInfo)
W_REGISTER_ARGTYPE(vst::VSTInfo)
Q_DECLARE_METATYPE(std::vector<vst::VSTInfo>)
W_REGISTER_ARGTYPE(std::vector<vst::VSTInfo>)


namespace vst
{

ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& app)
    : score::ApplicationPlugin{app}
    , m_wsServer("vst-notification-server", QWebSocketServer::NonSecureMode)
{
  qRegisterMetaType<VSTInfo>();
  qRegisterMetaTypeStreamOperators<VSTInfo>();
  qRegisterMetaType<std::vector<VSTInfo>>();
  qRegisterMetaTypeStreamOperators<std::vector<VSTInfo>>();

  m_wsServer.listen({}, 37587);
  con(m_wsServer, &QWebSocketServer::newConnection, this, [this] {
    QWebSocket* ws = m_wsServer.nextPendingConnection();
    if (!ws)
      return;

    connect(ws, &QWebSocket::textMessageReceived, this, [=](const QString& txt) {
      processIncomingMessage(txt);
      ws->deleteLater();
    });
  });
}

void ApplicationPlugin::initialize()
{
  // init with the database
  QSettings s;
  auto val = s.value("Effect/KnownVST2");
  if (val.canConvert<std::vector<VSTInfo>>())
  {
    vst_infos = val.value<std::vector<VSTInfo>>();
  }

  if (vst_invalid_format)
  {
    vst_infos.clear();
    vst_invalid_format = false;
  }

  vstChanged();

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::VstPathsChanged, this, &ApplicationPlugin::rescanVSTs);

  if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    rescanVSTs(set.getVstPaths());
}

void ApplicationPlugin::addInvalidVST(const QString& path)
{
  VSTInfo i;
  i.path = path;
  i.prettyName = "invalid";
  i.uniqueID = -1;
  i.isSynth = false;
  i.isValid = false;
  vst_infos.push_back(i);

  // write in the database
  QSettings{}.setValue("Effect/KnownVST2", QVariant::fromValue(vst_infos));

  vstChanged();
}

void ApplicationPlugin::addVST(const QString& path, const QJsonObject& obj)
{
  VSTInfo i;
  i.path = path;
  i.uniqueID = obj["UniqueID"].toInt();
  i.isSynth = obj["Synth"].toBool();
  i.author = obj["Author"].toString();
  i.displayName = obj["PrettyName"].toString();
  i.controls = obj["Controls"].toInt();
  i.isValid = true;

  // Only way to get a separation between Kontakt 5 / Kontakt 5 (8
  // out) / Kontakt 5 (16 out),  etc...
  i.prettyName = QFileInfo(path).baseName();

  vst_modules.insert({i.uniqueID, nullptr});
  vst_infos.push_back(std::move(i));

  // write in the database
  QSettings{}.setValue("Effect/KnownVST2", QVariant::fromValue(vst_infos));

  qDebug() << "Loaded VST " << path << "successfully";
  vstChanged();
}

void ApplicationPlugin::rescanVSTs(const QStringList& paths)
{
  // 1. List all plug-ins in new paths
  QSet<QString> newPlugins;
  for (QString dir : paths)
  {
#if defined(__APPLE__)
    {
      QDirIterator it(
          dir,
          QStringList{"*.vst", "*.component"},
          QDir::AllEntries,
          QDirIterator::Subdirectories);

      while (it.hasNext())
        newPlugins.insert(it.next());
    }
    {
      QDirIterator it(dir, QStringList{"*.dylib"}, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext())
      {
        auto path = it.next();
        if (!path.contains(".vst") && !path.contains(".component"))
          newPlugins.insert(path);
      }
    }
#else
    QDirIterator it(
        dir,
        QStringList{vst::default_filter},
        QDir::Files,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext())
      newPlugins.insert(it.next());
#endif
  }

  // 2. Remove plug-ins not in these paths
  for (auto it = vst_infos.begin(); it != vst_infos.end();)
  {
    auto new_it = newPlugins.find(it->path);
    if (new_it != newPlugins.end())
    {
      // plug-in is in both set, we ignore it
      newPlugins.erase(new_it);
      ++it;
    }
    else
    {
      it = vst_infos.erase(it);
    }
  }

  vstChanged();

  // 3. Add remaining plug-ins
  m_processes.clear();
  m_processes.reserve(newPlugins.size());
  int i = 0;
  for (const QString& path : newPlugins)
  {
    if (path.contains("linvst.so"))
      continue;

    auto proc = std::make_unique<QProcess>();

#if defined(__APPLE__)
    {
      QString bundle_vstpuppet = qApp->applicationDirPath() + "/ossia-score-vstpuppet.app/Contents/MacOS/ossia-score-vstpuppet";
      if(QFile::exists(bundle_vstpuppet))
        proc->setProgram(bundle_vstpuppet);
      else
        proc->setProgram(qApp->applicationDirPath() + "/ossia-score-vstpuppet");
    }
#else
    proc->setProgram("ossia-score-vstpuppet");
#endif
    proc->setArguments({path, QString::number(i)});
    m_processes.push_back({path, std::move(proc), false, {}});
    i++;
  }
  scanVSTsEvent();
}

void ApplicationPlugin::processIncomingMessage(const QString& txt)
{
  QJsonDocument doc = QJsonDocument::fromJson(txt.toUtf8());
  if (doc.isObject())
  {
    auto obj = doc.object();
    addVST(obj["Path"].toString(), obj);
    int id = obj["Request"].toInt();

    if(id >= 0 && id < m_processes.size())
    {
      if (m_processes[id].process)
      {
        m_processes[id].process->close();
        if (m_processes[id].process->state() == QProcess::ProcessState::NotRunning)
        {
          m_processes[id] = {};
        }
        else
        {
          connect(
                m_processes[id].process.get(),
                qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                this,
                [this, id] { m_processes[id] = {}; });
        }
      }
    }
    else
    {
      qDebug() << "Got invalid VST3 request ID" << id;
    }
  }
}

void ApplicationPlugin::scanVSTsEvent()
{
  constexpr int max_in_flight = 8;
  int in_flight = 0;

  for (auto& proc : m_processes)
  {
    // Already scanned processes
    if (!proc.process)
      continue;

    if (!proc.scanning)
    {
      proc.process->start(QProcess::ReadOnly);
      proc.scanning = true;
      proc.timer.start();
    }
    else
    {
      if (proc.timer.elapsed() > 10000)
      {
        addInvalidVST(proc.path);
        proc.process.reset();

        in_flight--;
        continue;
      }
    }

    in_flight++;

    if (in_flight == max_in_flight)
    {
      QTimer::singleShot(1000, this, &ApplicationPlugin::scanVSTsEvent);
      return;
    }
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  for (auto& e : vst_modules)
  {
    delete e.second;
  }
}

GUIApplicationPlugin::GUIApplicationPlugin(const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
{
}

void GUIApplicationPlugin::initialize() { }
}
