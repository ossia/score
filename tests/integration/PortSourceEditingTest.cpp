// Typing into a QML editor bound to a control port with Score.UI.PortSource,
// while the executor is publishing the port's value back.
//
// PortSource is two-way: the bound property drives the port, and
// Process::ControlInlet::executionValueChanged drives the property. During
// playback that second direction fires constantly and with stale data: avnd
// enqueues a snapshot of *every* control input as soon as any one of them
// changed (finish_run() in 3rdparty/avendish/include/avnd/binding/ossia/node.hpp)
// and the UI side pushes the whole tuple back through setExecutionValue()
// (src/plugins/score-plugin-avnd/Crousti/ExecutorUpdateControlValueInUi.hpp).
// The snapshot is the executor's state, so it is behind the keystroke that has
// not reached the executor yet.
//
// That lag is what this test reproduces: every keystroke is followed by the
// value the port held *before* it, exactly as a tick straddling the keystroke
// would deliver. The editor has to keep what was typed, and an editor nobody
// is typing into has to keep following the port -- an OSC or score-side write
// must still be visible in the UI.
//
// A real QML item and real key events, not a direct property write: the guard
// is about the keyboard focus, which only exists once the editor is in a
// window's focus chain.

#include <Process/Dataflow/Port.hpp>

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QObject>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QVariant>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Keyboard.hpp>

#include <memory>
#include <string>

namespace
{
// Two editors on two ports: one the test types into, one that only ever
// receives. TextInput is QQuickTextInput, the class TextField is built on, and
// TextArea -- koaia's prompt box, where this was found -- is its QQuickTextEdit
// sibling: both expose `text` and `activeFocus` the same way.
constexpr auto qml = R"qml(
import QtQuick
import Score.UI 1.0 as UI

Item {
  width: 320; height: 120

  TextInput {
    objectName: "typed"
    text: "origami"
    UI.PortSource on text { port: typedPort }
  }

  TextInput {
    objectName: "watched"
    y: 40
    text: "initial"
    UI.PortSource on text { port: watchedPort }
  }
}
)qml";

std::string text(QQuickItem& item)
{
  return item.property("text").toString().toStdString();
}

std::string portValue(const Process::ControlInlet& inlet)
{
  return ossia::convert<std::string>(inlet.value());
}
}

TEST_CASE(
    "a QML editor bound with PortSource keeps what is typed into it while the "
    "executor republishes the port",
    "[integration][js][gui][portsource]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext&) {
    Process::ControlInlet typedPort{"typed", Id<Process::Port>{0}, nullptr};
    Process::ControlInlet watchedPort{"watched", Id<Process::Port>{1}, nullptr};
    typedPort.setValue(std::string{"origami"});
    watchedPort.setValue(std::string{"initial"});

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(
        "typedPort", QVariant::fromValue<QObject*>(&typedPort));
    engine.rootContext()->setContextProperty(
        "watchedPort", QVariant::fromValue<QObject*>(&watchedPort));

    QQmlComponent component{&engine};
    component.setData(qml, QUrl{});
    INFO("QML: " << component.errorString().toStdString());
    std::unique_ptr<QObject> root{component.create()};
    REQUIRE(root != nullptr);

    auto* item = qobject_cast<QQuickItem*>(root.get());
    REQUIRE(item != nullptr);

    // QtQuick's activeFocus is the window's own focus chain, so the item has to
    // be in a window -- but not a shown one, which would need a render loop.
    QQuickWindow window;
    window.resize(320, 120);
    item->setParentItem(window.contentItem());

    auto* typed = item->findChild<QQuickItem*>("typed");
    auto* watched = item->findChild<QQuickItem*>("watched");
    REQUIRE(typed != nullptr);
    REQUIRE(watched != nullptr);

    typed->forceActiveFocus();
    QApplication::processEvents();
    if(!typed->hasActiveFocus())
      SKIP("QtQuick did not give the editor the active focus on this platform");
    CHECK_FALSE(watched->hasActiveFocus());

    // One keystroke at a time, each one followed by the port's pre-keystroke
    // value coming back from the executor.
    const QString typing = "ZQXVB";
    for(const QChar c : typing)
    {
      const auto stale = typedPort.value();
      score::test::keyClick(window, score::test::keyOf(c), Qt::NoModifier, QString{c});
      QApplication::processEvents();

      // The keystroke reached the port: that direction is not what changed.
      REQUIRE(portValue(typedPort) == text(*typed));

      typedPort.setExecutionValue(stale);
      QApplication::processEvents();
    }

    // Before the guard, the lagging snapshot overwrote the editor on every
    // keystroke and nothing typed survived.
    CHECK(text(*typed) == "origamiZQXVB");
    CHECK(portValue(typedPort) == "origamiZQXVB");

    // The other half of the contract: a port nobody is typing into still
    // reaches the UI. This is what koaia lost by dropping PortSource from its
    // typed inputs to work around the clobbering.
    watchedPort.setExecutionValue(std::string{"from the network"});
    QApplication::processEvents();
    CHECK(text(*watched) == "from the network");

    // Including while a *different* editor has the focus: the guard is
    // per-item, not global.
    CHECK(typed->hasActiveFocus());
    watchedPort.setExecutionValue(std::string{"second write"});
    QApplication::processEvents();
    CHECK(text(*watched) == "second write");

    // And once the user leaves the editor they were typing in, it follows the
    // port again.
    typed->setFocus(false);
    QApplication::processEvents();
    REQUIRE_FALSE(typed->hasActiveFocus());
    typedPort.setExecutionValue(std::string{"set from outside"});
    QApplication::processEvents();
    CHECK(text(*typed) == "set from outside");

    item->setParentItem(nullptr);
  });
}
