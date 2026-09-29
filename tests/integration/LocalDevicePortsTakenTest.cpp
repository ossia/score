// The local device's OSC / WebSocket ports already taken by another program:
// score says so, names the port and how to change it, and keeps running.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <LocalTree/Device/LocalSpecificSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ip/udp.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace asio = boost::asio;

//! Before the application exists: its local device reads the ports at startup.
struct scoped_env
{
  scoped_env(const char* name, const QByteArray& value)
      : m_name{name}
      , m_had{qEnvironmentVariableIsSet(name)}
      , m_old{qgetenv(name)}
  {
    qputenv(name, value);
  }
  ~scoped_env()
  {
    if(m_had)
      qputenv(m_name, m_old);
    else
      qunsetenv(m_name);
  }

private:
  const char* m_name;
  bool m_had;
  QByteArray m_old;
};

bool mentions(const QString& text, int port)
{
  return text.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(port)));
}

//! The warnings about the local device's ports: they tell how to change them.
QStringList g_warnings;
QtMessageHandler g_previous{};
void recordWarnings(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
  if(type == QtWarningMsg && msg.contains(QLatin1String("--local-osc-port")))
    g_warnings << msg;
  g_previous(type, ctx, msg);
}
}

TEST_CASE(
    "taken local device ports are reported and score keeps running",
    "[integration][localtree][network]")
{
  // Held the way another score instance holds them.
  asio::io_context io;
  asio::ip::udp::socket udp{io, asio::ip::udp::endpoint{asio::ip::udp::v4(), 0}};
  asio::ip::tcp::acceptor tcp{io, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), 0}};
  const int osc = udp.local_endpoint().port();
  const int ws = tcp.local_endpoint().port();
  scoped_env osc_env{"SCORE_LOCAL_OSC_PORT", QByteArray::number(osc)};
  scoped_env ws_env{"SCORE_LOCAL_WS_PORT", QByteArray::number(ws)};

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    g_warnings.clear();
    g_previous = qInstallMessageHandler(recordWarnings);

    QStringList boxes;
    QTimer closer;
    QObject::connect(&closer, &QTimer::timeout, [&] {
      if(auto box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
      {
        boxes << box->text();
        box->done(QMessageBox::Ok);
      }
    });
    closer.start(10);

    // Opening a document: the WebSocket port is taken, nothing is exposed.
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    REQUIRE(score::test::wait_until([&] { return !boxes.empty(); }));
    CHECK(boxes.size() == 1);
    CHECK(mentions(boxes[0], ws));
    CHECK(mentions(boxes[0], osc));
    REQUIRE(g_warnings.size() == 1);
    CHECK(g_warnings[0] == boxes[0]);

    auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>().list();
    auto local = devices.localDevice();
    REQUIRE(local);

    // The device edited to the same ports once only the UDP one is still
    // taken: OSC goes to another port, and says which.
    tcp.close();
    boxes.clear();
    g_warnings.clear();
    const auto settings = local->settings();
    local->updateSettings(settings);
    REQUIRE(score::test::wait_until([&] { return !boxes.empty(); }));
    CHECK(boxes.size() == 1);
    CHECK(mentions(boxes[0], osc));
    CHECK(!mentions(boxes[0], ws));
    REQUIRE(g_warnings.size() == 1);
    CHECK(g_warnings[0] == boxes[0]);

    // Still running: the document is there and takes a new one next to it.
    CHECK(ctx.docManager.documents().size() == 1);
    CHECK(score::test::new_document(ctx));
    CHECK(ctx.docManager.documents().size() == 2);

    closer.stop();
    qInstallMessageHandler(g_previous);
  });
}
