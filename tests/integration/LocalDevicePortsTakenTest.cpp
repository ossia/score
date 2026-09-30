// The local device's OSC / WebSocket ports already taken by another program:
// score says so, names the port and how to change it, and keeps running. Taken
// by another document of the same score (several open, a reload, a crash
// restore), they are not worth a word.

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

//! Closes the warning boxes as they show up, and keeps their text.
struct box_closer
{
  QStringList boxes;
  QTimer timer;
  box_closer()
  {
    QObject::connect(&timer, &QTimer::timeout, [this] {
      for(auto w : QApplication::topLevelWidgets())
        if(auto box = qobject_cast<QMessageBox*>(w); box && box->isVisible())
        {
          boxes << box->text();
          box->done(QMessageBox::Ok);
        }
    });
    timer.start(10);
  }
};

//! Ports nothing listens on right now.
std::pair<int, int> freePorts()
{
  asio::io_context io;
  asio::ip::udp::socket udp{io, asio::ip::udp::endpoint{asio::ip::udp::v4(), 0}};
  asio::ip::tcp::acceptor tcp{io, asio::ip::tcp::endpoint{asio::ip::tcp::v4(), 0}};
  return {udp.local_endpoint().port(), tcp.local_endpoint().port()};
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

    box_closer closer;
    auto& boxes = closer.boxes;

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

    qInstallMessageHandler(g_previous);
  });
}

TEST_CASE(
    "ports held by another open document are not reported as taken",
    "[integration][localtree][network]")
{
  const auto [osc, ws] = freePorts();
  scoped_env osc_env{"SCORE_LOCAL_OSC_PORT", QByteArray::number(osc)};
  scoped_env ws_env{"SCORE_LOCAL_WS_PORT", QByteArray::number(ws)};

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    g_warnings.clear();
    g_previous = qInstallMessageHandler(recordWarnings);
    box_closer closer;

    auto first = score::test::new_document(ctx);
    REQUIRE(first);

    // A second document, as File > New or a crash restore of several
    // documents opens: its local device finds the ports held by the first one.
    auto second = score::test::new_document(ctx);
    REQUIRE(second);
    CHECK(ctx.docManager.documents().size() == 2);

    // Its settings applied again: the device edited to the same ports.
    auto local
        = second->context().plugin<Explorer::DeviceDocumentPlugin>().list().localDevice();
    REQUIRE(local);
    local->updateSettings(local->settings());

    // The first document loaded again while it is open (the second one, still
    // untouched, makes way for it).
    auto third = score::test::reload_via_bytes(ctx, *first);
    REQUIRE(third);
    CHECK(ctx.docManager.documents().size() == 2);

    score::test::run_events_for(100);
    CHECK(closer.boxes.isEmpty());
    CHECK(g_warnings.isEmpty());

    // All closed, one opened again: the ports are free, nothing to say either.
    score::test::close_all_documents(ctx);
    REQUIRE(score::test::new_document(ctx));
    score::test::run_events_for(100);
    CHECK(closer.boxes.isEmpty());
    CHECK(g_warnings.isEmpty());

    qInstallMessageHandler(g_previous);
  });
}
