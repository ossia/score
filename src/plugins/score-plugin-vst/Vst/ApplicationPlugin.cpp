#include "ApplicationPlugin.hpp"

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Media/Effect/Settings/Model.hpp>
#include <Vst/EffectModel.hpp>
#include <Vst/Loader.hpp>

#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
#include <QWebSocket>

#include <wobjectimpl.h>
W_OBJECT_IMPL(vst::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(vst::VSTInfo)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<vst::VSTInfo>)
// TODO remove me in a few versions
static bool vst_invalid_format = false;
template <>
void DataStreamReader::read(const vst::VSTInfo& p)
{
  m_stream << p.path << p.prettyName << p.displayName << p.author << p.uniqueID
           << p.controls << p.isSynth << p.isValid;
}
template <>
void DataStreamWriter::write(vst::VSTInfo& p)
{
  if(!vst_invalid_format)
    m_stream >> p.path >> p.prettyName >> p.displayName >> p.author >> p.uniqueID
        >> p.controls >> p.isSynth >> p.isValid;
  if(m_stream.stream.status() != QDataStream::Status::Ok)
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
#if QT_CONFIG(process)
    , m_wsServer("vst-notification-server", QWebSocketServer::NonSecureMode)
#endif
{
  qRegisterMetaType<VSTInfo>();
  qRegisterMetaType<std::vector<VSTInfo>>();

#if QT_CONFIG(process)
  m_wsServer.listen(QHostAddress::LocalHost, 37587);
  con(m_wsServer, &QWebSocketServer::newConnection, this, [this] {
    QWebSocket* ws = m_wsServer.nextPendingConnection();
    if(!ws)
      return;

    connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString& txt) {
      processIncomingMessage(txt);
      ws->deleteLater();
    });
  });
#endif

  // VST idle update
  startTimer(10, Qt::PreciseTimer);
}

void ApplicationPlugin::initialize()
{
  // init with the database
  QSettings s;
  auto val = s.value("Effect/KnownVST2");
  if(val.canConvert<std::vector<VSTInfo>>())
  {
    vst_infos = val.value<std::vector<VSTInfo>>();
  }

  if(vst_invalid_format)
  {
    vst_infos.clear();
    vst_invalid_format = false;
  }

  vstChanged();

  auto& set = context.settings<Media::Settings::Model>();
  con(set, &Media::Settings::Model::VstPathsChanged, this,
      &ApplicationPlugin::rescanVSTs);

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
  i.prettyName = QFileInfo(path).completeBaseName();

  vst_modules.insert({i.uniqueID, nullptr});
  vst_infos.push_back(std::move(i));

  // write in the database
  QSettings{}.setValue("Effect/KnownVST2", QVariant::fromValue(vst_infos));

  qDebug() << "Loaded VST " << path << "successfully";
  vstChanged();
}

void ApplicationPlugin::registerRunningVST(Model* m)
{
  m_runningVSTs.push_back(m);
}

void ApplicationPlugin::unregisterRunningVST(Model* m)
{
  auto it = ossia::find(m_runningVSTs, m);
  if(it != m_runningVSTs.end())
  {
    m_runningVSTs.erase(it);
  }
}

static const QString& vstPuppetPath()
{
  static const QString path = []() -> QString {
    auto app = QCoreApplication::instance()->applicationDirPath();
#if defined(__APPLE__)
    auto bundle_path
        = "/ossia-score-vstpuppet.app/Contents/MacOS/"
          "ossia-score-vstpuppet";
    QString bundle_vstpuppet = app + bundle_path;
    if(QFile::exists(bundle_vstpuppet))
      return bundle_vstpuppet;
    else if(QFile::exists(app + "/ossia-score-vstpuppet"))
      return QString(app + "/ossia-score-vstpuppet");
    else if(QFile::exists(app + "/../../ossia-score-vstpuppet"))
      return QString(path + "/../../ossia-score-vstpuppet");
    else if(QFile::exists(app + "/../../" + bundle_path))
      return QString(app + "/../../" + bundle_path);
    else
      return QStringLiteral("ossia-score-vstpuppet");
#else
    return app + "/ossia-score-vstpuppet";
#endif
  }();
  return path;
}

void ApplicationPlugin::clearVSTs()
{
  vst_infos.clear();
  vstChanged();
}

void ApplicationPlugin::rescanVSTs(QStringList paths)
{
#if QT_CONFIG(process)
  // 0. Handle VST_PATH
  if(QFileInfo vst_env_path{QString(qgetenv("VST_PATH"))}; vst_env_path.isDir())
  {
    paths += vst_env_path.absoluteFilePath();
  }

  for(auto it = paths.begin(); it != paths.end();)
  {
    auto& path = *it;
    auto fi = QFileInfo{path};
    if(!fi.exists())
    {
      it = paths.erase(it);
    }
    else
    {
      path = QFileInfo{path}.canonicalFilePath();
      ++it;
    }
  }
  paths.removeDuplicates();

  // 1. List all plug-ins in new paths
  QStringList exploredPaths;
  QSet<QString> newPlugins;
  for(QString dir : paths)
  {
    auto canonical_path = QDir{dir}.canonicalPath();
    if(exploredPaths.contains(canonical_path))
      continue;

    exploredPaths.push_back(canonical_path);

#if defined(__APPLE__)
    {
      QDirIterator it(
          dir, QStringList{"*.vst", "*.component"}, QDir::AllEntries,
          QDirIterator::Subdirectories);

      while(it.hasNext())
        newPlugins.insert(it.next());
    }
    {
      QDirIterator it(
          dir, QStringList{"*.dylib"}, QDir::Files, QDirIterator::Subdirectories);
      while(it.hasNext())
      {
        auto path = it.next();
        if(!path.contains(".vst") && !path.contains(".component"))
          newPlugins.insert(path);
      }
    }
#else
    QDirIterator it(
        dir, QStringList{vst::default_filter}, QDir::Files,
        QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while(it.hasNext())
      newPlugins.insert(it.next());
#endif
  }

  // 2. Remove plug-ins not in these paths
  for(auto it = vst_infos.begin(); it != vst_infos.end();)
  {
    auto new_it = newPlugins.find(it->path);
    if(new_it != newPlugins.end())
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
  for(const QString& path : newPlugins)
  {
    if(path.contains("linvst.so"))
      continue;

    auto proc = std::make_unique<QProcess>();
    connect(proc.get(), &QProcess::errorOccurred, this, [proc = proc.get(), path] {
      qDebug() << " == VST: error => " << path;
      qDebug() << " -- VST out: " << proc->readAllStandardOutput().constData();
      qDebug() << " -- VST error: " << proc->readAllStandardError().constData();
    });

    proc->setProgram(vstPuppetPath());
    proc->setArguments({path, QString::number(i)});
    m_processes.push_back({path, std::move(proc), false, {}});
    i++;
  }
  scanVSTsEvent();
#endif
}

static int vst_in_flight = 0;
void ApplicationPlugin::processIncomingMessage(const QString& txt)
{
#if QT_CONFIG(process)
  QJsonDocument doc = QJsonDocument::fromJson(txt.toUtf8());
  if(doc.isObject())
  {
    auto obj = doc.object();
    addVST(obj["Path"].toString(), obj);
    int id = obj["Request"].toInt();

    if(id >= 0 && id < std::ssize(m_processes))
    {
      if(m_processes[id].process)
      {
        m_processes[id].process->close();
        if(m_processes[id].process->state() == QProcess::ProcessState::NotRunning)
        {
          vst_in_flight--;
          m_processes[id] = {};
        }
        else
        {
          connect(
              m_processes[id].process.get(),
              qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
              [this, id] {
            vst_in_flight--;
            m_processes[id] = {};
          });
        }
      }
    }
    else
    {
      qDebug() << "Got invalid VST request ID" << id;
    }
  }
#endif
}

void ApplicationPlugin::scanVSTsEvent()
{
#if QT_CONFIG(process)
  constexpr int max_in_flight = 8;

  for(auto& proc : m_processes)
  {
    // Already scanned processes
    if(!proc.process)
      continue;

    if(!proc.scanning)
    {
      if(vst_in_flight < max_in_flight)
      {
        proc.process->start(QProcess::ReadOnly);
        proc.scanning = true;
        proc.timer.start();

        vst_in_flight++;
      }
    }
    else
    {
      if(proc.timer.elapsed() > 10000)
      {
        addInvalidVST(proc.path);
        proc.process->kill();
        proc.process.reset();

        vst_in_flight--;
        continue;
      }
    }

    if(vst_in_flight == max_in_flight)
    {
      QTimer::singleShot(1000, this, &ApplicationPlugin::scanVSTsEvent);
      return;
    }
  }
#endif
}

void ApplicationPlugin::timerEvent(QTimerEvent* event)
{
  for(auto vst : m_runningVSTs)
  {
    if(vst->needIdle.exchange(false, std::memory_order_acquire))
    {
      vst->dispatch(effEditIdle);
    }
  }
}

ApplicationPlugin::~ApplicationPlugin()
{
  for(auto& e : vst_modules)
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
