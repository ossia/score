// The WebSocket device's settings read the host out of the device script when
// its edit ends. QCodeEditor's editingFinished is an inline signal: connected
// from the protocols plugin, it named that plugin's hidden copy, which the
// editor's meta-object does not know, so the connection failed ("signal not
// found") and the address was never filled in.

#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QFocusEvent>
#include <QLineEdit>
#include <QTextEdit>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

#include <memory>

TEST_CASE("the end of a script edit fills the WebSocket address", "[websocket][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    QStringList failedConnects;
    static QStringList* sink{};
    sink = &failedConnects;
    auto previous = qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext&, const QString& msg) {
      if(msg.contains(QLatin1String("signal not found")))
        sink->push_back(msg);
    });

    // WSProtocolFactory
    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        UuidKey<Device::ProtocolFactory>{"59e81303-af24-4559-b33d-1c6f59f0f017"});
    REQUIRE(factory);
    std::unique_ptr<Device::ProtocolSettingsWidget> widget{
        factory->makeSettingsWidget()};
    REQUIRE(widget);
    qInstallMessageHandler(previous);
    CHECK(failedConnects.isEmpty());

    auto* code = widget->findChild<QTextEdit*>();
    REQUIRE(code);

    // The editor only counts a change typed while it has the focus.
    widget->show();
    widget->activateWindow();
    code->setFocus();
    QElapsedTimer focus;
    focus.start();
    while(!code->hasFocus() && focus.elapsed() < 5000)
      QApplication::processEvents(QEventLoop::AllEvents, 20);
    REQUIRE(code->hasFocus());
    code->setPlainText(QStringLiteral(
        "import QtQml\nQtObject { property string host: \"ws://127.0.0.1:9999\" }"));
    code->clearFocus();

    const auto hostShown = [&] {
      for(auto* e : widget->findChildren<QLineEdit*>())
        if(e->text() == QLatin1String("ws://127.0.0.1:9999"))
          return true;
      return false;
    };
    QElapsedTimer t;
    t.start();
    while(!hostShown() && t.elapsed() < 5000)
      QApplication::processEvents(QEventLoop::AllEvents, 20);
    CHECK(hostShown());
  });
}
