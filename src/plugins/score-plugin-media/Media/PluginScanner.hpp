#pragma once

// Shared out-of-process plug-in scan machinery for the VST2 / VST3 / CLAP /
// LV2 application plug-ins. One puppet process is spawned per plug-in file
// (a crashing plug-in must not take score down); the puppet reports back
// over a local WebSocket.
//
// Design points, distilled from years of per-backend bugs:
//
//  * The server listens on an *ephemeral* port and every reply must echo a
//    per-scanner random token. The old fixed ports (37587..37590) meant
//    that when several score-derived processes ran at once, every scan
//    reply landed in whichever instance owned the port: that instance
//    appended (and persisted) duplicates of every plug-in on each run,
//    while the scanning instance timed out and marked its plug-ins
//    invalid. Replies with a wrong/missing token are dropped before they
//    can touch any plug-in database.
//  * Event-driven refill: a fixed pool of at most maxInFlight() live
//    puppets, refilled whenever one resolves. (The previous VST2/VST3
//    implementation polled on a 1s timer, kept the in-flight count in a
//    translation-unit static that leaked on rescan, and only checked
//    timeouts while saturated - hung puppets in the last batch leaked
//    forever.)
//  * A puppet's exit and the WebSocket delivery of its reply race each
//    other: the process routinely finishes - sometimes with a non-zero
//    exit code from the close-handshake - before the reply is dispatched.
//    A finished-without-reply record is therefore kept in a short grace
//    period instead of being declared failed on the spot; failure is only
//    reported if the grace period elapses with no reply. This subsumes the
//    LV2 m_scanned_ok workaround and fixes the CLAP double-invalid.
//  * scanFailed() is emitted at most once per path, whatever combination
//    of errorOccurred / finished / timeout fires.

#include <score_plugin_media_export.h>

#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStringList>

#include <verdigris>

#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <QJsonObject>

class QWebSocketServer;

namespace Media
{
class SCORE_PLUGIN_MEDIA_EXPORT PluginScanner : public QObject
{
  W_OBJECT(PluginScanner)
public:
  //! serverName is only a debugging label for the WebSocket server.
  explicit PluginScanner(QString serverName, QObject* parent = nullptr);
  ~PluginScanner();

  void setPuppet(const QString& executable);
  void setMaxInFlight(int n);
  //! How long a puppet may run before it is killed.
  void setProcessTimeout(int ms);
  //! How long to keep waiting for the WebSocket reply of a process that
  //! already exited (the reply delivery races the process exit).
  void setReplyGracePeriod(int ms);
  //! Extra environment for the puppets; system environment by default.
  void setEnvironmentProvider(std::function<QProcessEnvironment()> f);

  //! Start scanning the given plug-in files. A scan already in progress is
  //! cancelled first: its puppets are reaped and emit nothing.
  void scan(QStringList pluginPaths);

  bool scanning() const noexcept;

  //! The per-scanner secret that replies must echo. Exposed for tests.
  const QString& token() const noexcept { return m_token; }
  //! The port puppets are told to connect to; 0 until the first scan.
  quint16 port() const noexcept;

  //! Feed one reply as if it had arrived on the socket. Public so that the
  //! protocol validation (token, request id form) is directly testable.
  void processIncomingMessage(const QString& message);

  //! A plug-in file was scanned; obj is the puppet's reply. The path is the
  //! one the scanner was asked to scan - not whatever the reply claims.
  void scanned(QString path, QJsonObject obj)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, scanned, path, obj);
  //! The puppet crashed, timed out, errored out, or replied with "Error".
  void scanFailed(QString path, QString reason)
      E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, scanFailed, path, reason);
  //! All requested paths have been resolved one way or the other.
  void done() E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, done);

private:
  struct Record
  {
    QString path;
    QPointer<QProcess> process;
    bool live{};    // started and not yet finished/reaped
    bool replied{}; // a token-valid reply was attributed to it
  };

  bool ensureListening();
  void refill();
  void startOne(const QString& path);
  void resolveProcess(int id);
  void releaseProcess(QProcess* proc);
  void beginGracePeriod(int id);
  void failIfStillUnresolved(int id, const QString& reason);
  void checkDone();
  void cancelCurrentScan();

  std::unique_ptr<QWebSocketServer> m_server;
  QString m_serverName;
  QString m_token;
  QString m_puppet;
  std::function<QProcessEnvironment()> m_env;

  std::map<int, Record> m_records;
  std::vector<QString> m_queue;
  int m_nextId{};
  int m_live{};
  int m_maxInFlight{8};
  int m_timeoutMs{10000};
  int m_graceMs{2000};
  bool m_scanRunning{};
};
}
