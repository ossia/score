// A stand-in for the plug-in scanner puppets, driven by the
// FAKE_PUPPET_BEHAVIOR environment variable. Speaks the same protocol:
//   fake_puppet <path> <id> <port> <token>
// and replies on ws://127.0.0.1:<port>.
//
// Behaviors:
//   reply        (default) send a well-formed reply, exit 0
//   reply_exit1  send a well-formed reply, then exit 1 (close-handshake race)
//   badtoken     send a reply carrying the wrong session token
//   stringid     send the request id as a JSON string (legacy puppets)
//   error        send an {"Error": ...} reply
//   garbage      send something that is not JSON
//   hang         connect but never reply (must be reaped by the timeout)
//   crash        abort() before replying
//   exit1        exit(1) without replying

#include <QCoreApplication>
#include <QTimer>
#include <QWebSocket>

#include <cstdlib>
#include <cstring>

int main(int argc, char** argv)
{
  QCoreApplication app(argc, argv);

  if(argc < 5)
    return 2;

  const QString path = argv[1];
  const QString id = argv[2];
  const int port = atoi(argv[3]);
  const QString token = argv[4];

  const char* behavior_env = getenv("FAKE_PUPPET_BEHAVIOR");
  const QString behavior = behavior_env ? behavior_env : "reply";

  if(behavior == "crash")
    abort();
  if(behavior == "exit1")
    return 1;

  auto ws = new QWebSocket;
  QObject::connect(ws, &QWebSocket::connected, &app, [&] {
    if(behavior == "hang")
      return; // stay connected, never reply

    QString msg;
    if(behavior == "garbage")
    {
      msg = "this is not json {{{";
    }
    else
    {
      const QString tok = (behavior == "badtoken") ? "WRONG-TOKEN" : token;
      const QString req
          = (behavior == "stringid") ? ("\"" + id + "\"") : id;
      if(behavior == "error")
      {
        msg = QString(R"({"Path":"%1","Request":%2,"Token":"%3","Error":"simulated failure"})")
                  .arg(path, req, tok);
      }
      else
      {
        msg = QString(
                  R"({"Path":"%1","Request":%2,"Token":"%3","Name":"Fake Plugin","Plugins":[{"ID":"org.fake.%4","Name":"Fake %4"}]})")
                  .arg(path, req, tok, id);
      }
    }
    ws->sendTextMessage(msg);
    ws->flush();

    QTimer::singleShot(100, &app, [&] {
      if(behavior == "reply_exit1")
        std::exit(1);
      app.exit(0);
    });
  });

  // QWebSocket::errorOccurred only exists since Qt 6.5 (before that the
  // signal is the overloaded `error`); Coverage CI builds with Qt 6.4
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  auto error_signal = &QWebSocket::errorOccurred;
#else
  auto error_signal
      = qOverload<QAbstractSocket::SocketError>(&QWebSocket::error);
#endif
  QObject::connect(ws, error_signal, &app, [&](QAbstractSocket::SocketError) {
    // No server: mirrors the real puppets' "socket error" exit
    std::exit(1);
  });

  ws->open(QUrl(QString("ws://127.0.0.1:%1").arg(port)));

  // Global watchdog so a stuck test cannot leak processes forever
  QTimer::singleShot(30000, &app, [] { std::exit(3); });

  return app.exec();
}
