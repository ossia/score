#include "ApplicationPlugin.hpp"

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/tools/Bind.hpp>

#include <QApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QWebSocket>

#include <clap/all.h>

#include <wobjectimpl.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

W_OBJECT_IMPL(Clap::ApplicationPlugin)

SCORE_SERALIZE_DATASTREAM_DEFINE(Clap::PluginInfo)
SCORE_SERALIZE_DATASTREAM_DEFINE(std::vector<Clap::PluginInfo>)

template <>
void DataStreamReader::read(const Clap::PluginInfo& p)
{
  m_stream << p.path << p.id << p.name << p.vendor << p.version << p.url << p.manual_url
           << p.support_url << p.description << p.features << p.valid;
}
template <>
void DataStreamWriter::write(Clap::PluginInfo& p)
{
  m_stream >> p.path >> p.id >> p.name >> p.vendor >> p.version >> p.url >> p.manual_url
      >> p.support_url >> p.description >> p.features >> p.valid;
}
namespace Clap
{

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : score::GUIApplicationPlugin{app}
    , m_wsServer("clap-notification-server", QWebSocketServer::NonSecureMode)
{
  qRegisterMetaType<PluginInfo>();
  qRegisterMetaType<std::vector<PluginInfo>>();

  m_wsServer.listen(QHostAddress::LocalHost, 37589);
  con(m_wsServer, &QWebSocketServer::newConnection, this, [this] {
    QWebSocket* ws = m_wsServer.nextPendingConnection();
    if(!ws)
      return;

    connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString& txt) {
      processIncomingMessage(txt);
      ws->deleteLater();
    });
  });
}

ApplicationPlugin::~ApplicationPlugin()
{
  for(auto& proc : m_processes)
  {
    if(proc.second)
    {
      proc.second->terminate();
      if(!proc.second->waitForFinished(100))
        proc.second->kill();
    }
  }
}

void ApplicationPlugin::initialize()
{
  // Load cached plugin database
  QSettings s;
  auto val = s.value("Effect/KnownCLAP");
  if(val.canConvert<std::vector<PluginInfo>>())
  {
    m_plugins = val.value<std::vector<PluginInfo>>();
  }

  pluginsChanged();
  
  if(qEnvironmentVariableIsEmpty("SCORE_DISABLE_AUDIOPLUGINS"))
    rescanPlugins();
}

void ApplicationPlugin::rescanPlugins()
{
  // Clear previous scan state
  m_pluginQueue.clear();
  m_processes.clear();
  m_processCount = 0;
  
  // Collect all CLAP files to scan
  std::vector<QString> pluginsToScan;
  
  // Standard CLAP plugin directories
  QStringList paths;

#if defined(__APPLE__)
  paths << "/Library/Audio/Plug-Ins/CLAP"
        << (QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
            + "/Library/Audio/Plug-Ins/CLAP");
#elif defined(_WIN32)
  paths << "C:/Program Files/Common Files/CLAP"
        << QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
               + "/CLAP";
#else
  paths << QStringLiteral("/usr/lib/clap") << QStringLiteral("/usr/local/lib/clap")
        << QStringLiteral("/usr/lib64/clap") << QStringLiteral("/usr/local/lib64/clap");
#endif

  paths << QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.clap";
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
  ossia::hash_set<QString> known_plugins_paths;
  for(auto& plug : this->m_plugins)
    known_plugins_paths.insert(plug.path);

  for(const QString& searchPath : paths)
  {
    QDir dir(searchPath);
    if(!dir.exists())
      continue;

    QStringList filters;
    filters << "*.clap";

    for(const QString& fileName : dir.entryList(filters, QDir::Files | QDir::Dirs))
    {
      QString fullPath = dir.absoluteFilePath(fileName);
      if(known_plugins_paths.contains(fullPath))
        continue;

#if defined(__APPLE__)
      if(QFileInfo{fullPath}.isDir())
        fullPath = fullPath
                   + QString{"/Contents/MacOS/%1"}.arg(QFileInfo{fileName}.baseName());
#endif
      pluginsToScan.push_back(fullPath);
    }
  }
  
  // Queue all plugins for scanning
  for(const auto& path : pluginsToScan)
  {
    m_pluginQueue.push_back(path);
  }
  
  // Start scanning processes
  scanNextBatch();
}

void ApplicationPlugin::scanNextBatch()
{
  // Check if we're done
  if(m_pluginQueue.empty() && m_processes.empty())
  {
    // Save to settings
    QSettings{}.setValue("Effect/KnownCLAP", QVariant::fromValue(m_plugins));
    pluginsChanged();
    return;
  }

  // Launch new processes up to max_in_flight
  while(!m_pluginQueue.empty() && m_processes.size() < max_in_flight)
  {
    QString pluginPath = m_pluginQueue.back();
    m_pluginQueue.pop_back();

    int id = m_processCount++;
    auto proc = new QProcess;
    m_processes[id] = proc;

    connect(proc, &QProcess::finished, this, [this, id, pluginPath, proc](int exitCode, QProcess::ExitStatus) {
      m_processes.erase(id);
      proc->deleteLater();
      
      if(exitCode != 0)
      {
        qDebug() << "CLAP scan failed for:" << pluginPath;
        addInvalidPlugin(pluginPath);
      }

      // Continue scanning
      scanNextBatch();
    });

    connect(proc, &QProcess::errorOccurred, this, [this, id, pluginPath, proc](QProcess::ProcessError err) {
      qDebug() << "CLAP scan error for:" << pluginPath << "error:" << err;
      m_processes.erase(id);
      proc->deleteLater();
      addInvalidPlugin(pluginPath);
      scanNextBatch();
    });

    // Set up timeout
    auto timer = new QTimer;
    timer->setSingleShot(true);
    timer->setInterval(10000); // 10 second timeout
    connect(timer, &QTimer::timeout, proc, [this, id, proc, timer, pluginPath] {
      if(m_processes.find(id) != m_processes.end())
      {
        qDebug() << "CLAP scan timeout for:" << pluginPath;
        proc->terminate();
        if(!proc->waitForFinished(100))
          proc->kill();
      }
      timer->deleteLater();
    });
    connect(proc, &QProcess::finished, timer, &QTimer::deleteLater);
    timer->start();

    // Launch the puppet process
    QString program = QCoreApplication::applicationDirPath() + "/ossia-score-clappuppet";
#ifdef _WIN32
    program += ".exe";
#endif
    proc->start(program, {pluginPath, QString::number(id)});
  }
}

void ApplicationPlugin::processIncomingMessage(const QString& txt)
{
  try
  {
    auto json = QJsonDocument::fromJson(txt.toUtf8());
    if(!json.isObject())
      return;

    auto obj = json.object();
    auto req = obj["Request"].toInt();
    for(auto it = m_processes.begin(); it != m_processes.end();)
    {
      if(it->first == req)
      {
        auto proc = it->second;
        QObject::disconnect(it->second, nullptr, this, nullptr);
        proc->terminate();
        if(proc)
          if(!proc->waitForFinished(100))
            proc->kill();
        proc->deleteLater();
        it = m_processes.erase(it);
        break;
      }
      else
      {
        ++it;
      }
    }

    auto path = obj["Path"].toString();

    if(path.isEmpty())
      return;

    // Check if this is a valid plugin response
    if(obj.contains("Plugins") && obj["Plugins"].isArray())
    {
      auto plugins = obj["Plugins"].toArray();
      for(const auto& plugin : plugins)
      {
        if(plugin.isObject())
        {
          addPlugin(path, plugin.toObject());
        }
      }
    }
    else
    {
      addInvalidPlugin(path);
    }
  }
  catch(...)
  {
    qDebug() << "Failed to parse CLAP scan result";
  }
}

void ApplicationPlugin::addPlugin(const QString& path, const QJsonObject& obj)
{
  PluginInfo info;
  info.path = path;
  info.id = obj["ID"].toString();
  info.name = obj["Name"].toString();
  info.vendor = obj["Vendor"].toString();
  info.version = obj["Version"].toString();
  info.url = obj["URL"].toString();
  info.manual_url = obj["ManualURL"].toString();
  info.support_url = obj["SupportURL"].toString();
  info.description = obj["Description"].toString();

  if(obj.contains("Features") && obj["Features"].isArray())
  {
    auto features = obj["Features"].toArray();
    for(const auto& feature : features)
    {
      if(feature.isString())
        info.features.push_back(feature.toString());
    }
  }
  info.valid = true;

  m_plugins.push_back(std::move(info));
  // write in the database
  QSettings{}.setValue("Effect/KnownCLAP", QVariant::fromValue(m_plugins));

  pluginsChanged();
  scanNextBatch();
}

void ApplicationPlugin::addInvalidPlugin(const QString& path)
{
  PluginInfo info;
  info.path = path;
  info.name = "<Invalid>";
  info.valid = false;
  m_plugins.push_back(info);
  QSettings{}.setValue("Effect/KnownCLAP", QVariant::fromValue(m_plugins));

  pluginsChanged();
  scanNextBatch();
}
}
