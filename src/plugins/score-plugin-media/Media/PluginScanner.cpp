#include "PluginScanner.hpp"

#if QT_CONFIG(process)
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUuid>
#if __has_include(<QWebSocketServer>)
#include <QWebSocket>
#include <QWebSocketServer>
#else
#define SCORE_PLUGIN_MEDIA_NO_WEBSOCKETS 1
#endif

#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::PluginScanner)

namespace Media
{
namespace
{
//! Old puppets sent the request id as a JSON string ("Request":"3"); the
//! current ones send a number. QJsonValue::toInt() on a string silently
//! returns 0, which used to attribute every reply to scan slot 0.
int readRequestId(const QJsonValue& v) noexcept
{
  if(v.isDouble())
    return v.toInt(-1);
  if(v.isString())
  {
    bool ok{};
    const int res = v.toString().toInt(&ok);
    return ok ? res : -1;
  }
  return -1;
}
}

PluginScanner::PluginScanner(QString serverName, QObject* parent)
    : QObject{parent}
    , m_serverName{std::move(serverName)}
    , m_token{QUuid::createUuid().toString(QUuid::WithoutBraces)}
{
}

PluginScanner::~PluginScanner()
{
  cancelCurrentScan();
#if !defined(SCORE_PLUGIN_MEDIA_NO_WEBSOCKETS)
  delete m_server;
#endif
}

void PluginScanner::setPuppet(const QString& executable)
{
  m_puppet = executable;
}

void PluginScanner::setMaxInFlight(int n)
{
  m_maxInFlight = std::max(1, n);
}

void PluginScanner::setProcessTimeout(int ms)
{
  m_timeoutMs = ms;
}

void PluginScanner::setReplyGracePeriod(int ms)
{
  m_graceMs = ms;
}

void PluginScanner::setEnvironmentProvider(std::function<QProcessEnvironment()> f)
{
  m_env = std::move(f);
}

bool PluginScanner::scanning() const noexcept
{
  return m_scanRunning;
}

quint16 PluginScanner::port() const noexcept
{
#if defined(SCORE_PLUGIN_MEDIA_NO_WEBSOCKETS)
  return 0;
#else
  return m_server ? m_server->serverPort() : 0;
#endif
}

bool PluginScanner::ensureListening()
{
#if defined(SCORE_PLUGIN_MEDIA_NO_WEBSOCKETS)
  return false;
#else
  if(m_server && m_server->isListening())
    return true;

  m_server = new QWebSocketServer(m_serverName, QWebSocketServer::NonSecureMode);

  // Ephemeral port: several score-derived processes can scan concurrently
  // without ever seeing each other's replies.
  if(!m_server->listen(QHostAddress::LocalHost, 0))
  {
    qWarning() << m_serverName << ": failed to start scan server -"
               << m_server->errorString() << "- plug-in scanning unavailable";
    delete m_server;
    m_server = nullptr;
    return false;
  }

  connect(m_server, &QWebSocketServer::newConnection, this, [this] {
    QWebSocket* ws = m_server->nextPendingConnection();
    if(!ws)
      return;

    // Default Qt limits reject the multi-MB payloads of large bundles
    // (lsp-plugins ships hundreds of plug-ins in one file)
    ws->setMaxAllowedIncomingFrameSize(64 * 1024 * 1024);
    ws->setMaxAllowedIncomingMessageSize(64 * 1024 * 1024);

    // A puppet that connects but never replies (hang, crash, timeout kill)
    // would otherwise keep its socket alive for the scanner's lifetime
    connect(ws, &QWebSocket::disconnected, ws, &QObject::deleteLater);

    connect(ws, &QWebSocket::textMessageReceived, this, [this, ws](const QString& txt) {
      QObject::disconnect(ws, &QWebSocket::textMessageReceived, nullptr, nullptr);
      processIncomingMessage(txt);
      // Defer deleteLater so the puppet gets a chance to finish its own
      // teardown first (exit codes after a reply are ignored either way;
      // the disconnected handler below usually reaps the socket earlier)
      QTimer::singleShot(1000, ws, [ws] { ws->deleteLater(); });
    });
  });
  return true;
#endif
}

void PluginScanner::scan(QStringList pluginPaths)
{
  cancelCurrentScan();
  m_scanRunning = true;

  if(pluginPaths.isEmpty())
  {
    checkDone();
    return;
  }

  if(m_puppet.isEmpty() || !ensureListening())
  {
    // No way to run any puppet: resolve everything as failed so callers
    // are not left waiting for a done() that never comes.
    for(const auto& path : pluginPaths)
      scanFailed(path, QStringLiteral("plug-in scanner unavailable"));
    checkDone();
    return;
  }
  m_queue.assign(pluginPaths.rbegin(), pluginPaths.rend()); // pop_back order
  refill();
}

void PluginScanner::refill()
{
  while(!m_queue.empty() && m_live < m_maxInFlight)
  {
    QString path = std::move(m_queue.back());
    m_queue.pop_back();
    startOne(path);
  }
}

void PluginScanner::startOne(const QString& path)
{
  const int id = m_nextId++;

  auto proc = new QProcess;
  auto& rec = m_records[id];
  rec.path = path;
  rec.process = proc;
  rec.live = true;
  m_live++;

  if(m_env)
    proc->setProcessEnvironment(m_env());

  // stderr passes through for debugging; stdout stays piped (some puppets
  // echo their JSON there) and is discarded with the process
  proc->setProcessChannelMode(QProcess::ForwardedErrorChannel);

  connect(
      proc, &QProcess::errorOccurred, this, [this, id](QProcess::ProcessError err) {
    auto it = m_records.find(id);
    if(it == m_records.end() || !it->second.live)
      return;

    if(err == QProcess::FailedToStart)
    {
      // finished() will never fire; resolve here. Deferred: FailedToStart is
      // emitted synchronously from QProcess::start(), so resolving inline
      // would recurse start -> errorOccurred -> refill -> start through the
      // whole remaining queue on one stack (e.g. when the puppet binary is
      // missing).
      qDebug() << m_serverName << ": puppet failed to start for"
               << it->second.path;
      QTimer::singleShot(0, this, [this, id] {
        resolveProcess(id);
        failIfStillUnresolved(id, QStringLiteral("failed to start"));
        refill();
        checkDone();
      });
    }
    // Crashed also triggers finished(): a single resolution happens there
      });

  // Parented to proc so it dies with it, but connected in our context so
  // that destroying the scanner severs the connection
  auto timer = new QTimer{proc};
  timer->setSingleShot(true);
  timer->setInterval(m_timeoutMs);
  connect(timer, &QTimer::timeout, this, [this, id, proc] {
    auto it = m_records.find(id);
    if(it != m_records.end() && it->second.live)
    {
      qDebug() << m_serverName << ": scan timeout for" << it->second.path;
      proc->terminate();
      if(!proc->waitForFinished(100))
        proc->kill();
      // finished() fires next and resolves the record
    }
  });

  connect(
      proc, &QProcess::finished, this,
      [this, id](int exitCode, QProcess::ExitStatus status) {
    auto it = m_records.find(id);
    if(it == m_records.end() || !it->second.live)
      return;

    resolveProcess(id);

    if(it->second.replied)
    {
      m_records.erase(it);
    }
    else
    {
      // The reply races the process exit - even a non-zero exit code does
      // not prove failure (close-handshake teardown). Give the WebSocket
      // delivery a grace period before declaring the plug-in dead.
      (void)exitCode;
      (void)status;
      beginGracePeriod(id);
    }

    refill();
    checkDone();
      });

  timer->start();
  proc->start(
      m_puppet,
      {path, QString::number(id), QString::number(port()), m_token},
      QIODevice::ReadOnly);
}

//! Mark the process slot as free and schedule the QProcess for deletion.
void PluginScanner::resolveProcess(int id)
{
  auto it = m_records.find(id);
  if(it == m_records.end())
    return;
  auto& rec = it->second;
  if(rec.live)
  {
    rec.live = false;
    m_live--;
  }
  if(auto proc = rec.process.data())
  {
    rec.process.clear();
    releaseProcess(proc);
  }
}

void PluginScanner::releaseProcess(QProcess* proc)
{
  QObject::disconnect(proc, nullptr, this, nullptr);
  if(proc->state() != QProcess::NotRunning)
  {
    proc->terminate();
    // Reap after kill: ~QProcess on a still-running process re-kills and
    // blocks for up to 30s
    QTimer::singleShot(100, proc, [proc] {
      if(proc->state() != QProcess::NotRunning)
      {
        proc->kill();
        proc->waitForFinished(100);
      }
      proc->deleteLater();
    });
  }
  else
  {
    proc->deleteLater();
  }
}

void PluginScanner::beginGracePeriod(int id)
{
  QTimer::singleShot(m_graceMs, this, [this, id] {
    // One extra event-loop iteration before declaring failure: when the main
    // thread was stalled past the grace period (startup GUI build, large
    // QSettings rewrite), this timer and the WebSocket delivering the reply
    // become ready in the *same* poll iteration, and glib does not order a
    // timeout source before a socket source. Deferring by a 0ms timer lets a
    // reply that is already sitting in the socket win the race.
    QTimer::singleShot(0, this, [this, id] {
      failIfStillUnresolved(id, QStringLiteral("no reply from scanner process"));
      checkDone();
    });
  });
}

void PluginScanner::failIfStillUnresolved(int id, const QString& reason)
{
  auto it = m_records.find(id);
  if(it == m_records.end() || it->second.replied)
    return;

  const QString path = it->second.path;
  m_records.erase(it);
  scanFailed(path, reason);
}

void PluginScanner::checkDone()
{
  if(m_scanRunning && m_queue.empty() && m_records.empty())
  {
    m_scanRunning = false;
    done();
  }
}

void PluginScanner::processIncomingMessage(const QString& message)
{
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(message.toUtf8(), &err);
  if(!doc.isObject())
  {
    qWarning() << m_serverName << ": malformed scan reply (" << message.size()
               << "chars):" << err.errorString();
    return;
  }

  const auto obj = doc.object();

  // The token check is what keeps other score instances' scans - and any
  // other local process - out of our plug-in database.
  if(obj[QStringLiteral("Token")].toString() != m_token)
  {
    qWarning() << m_serverName << ": dropping scan reply with wrong session token";
    return;
  }

  const int id = readRequestId(obj[QStringLiteral("Request")]);
  auto it = m_records.find(id);
  if(it == m_records.end())
  {
    qWarning() << m_serverName << ": dropping scan reply for unknown request" << id;
    return;
  }

  auto& rec = it->second;
  if(rec.replied)
    return;
  rec.replied = true;

  const QString path = rec.path;
  const bool wasLive = rec.live;

  if(wasLive)
    resolveProcess(id);
  m_records.erase(id);

  if(obj.contains(QStringLiteral("Error")))
    scanFailed(path, obj[QStringLiteral("Error")].toString());
  else
    scanned(path, obj);

  refill();
  checkDone();
}

void PluginScanner::cancelCurrentScan()
{
  m_queue.clear();

  // waitForFinished delivers finished() synchronously: detach the map and
  // drop our connections first or the handlers would mutate it mid-iteration.
  auto records = std::move(m_records);
  m_records.clear();
  m_live = 0;
  m_scanRunning = false;

  for(auto& [id, rec] : records)
  {
    if(auto proc = rec.process.data())
    {
      QObject::disconnect(proc, nullptr, this, nullptr);
      proc->terminate();
      if(!proc->waitForFinished(100))
      {
        proc->kill();
        proc->waitForFinished(100);
      }
      delete proc;
    }
  }
}
}
#endif
