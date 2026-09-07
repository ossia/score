// score::QGraphicsCombo's drop-down editor: the popup that a right click puts
// in the scene, and what it does with a value that is not in the list.

#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/layouts/GraphicsTabLayout.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/widgets/ComboBox.hpp>

#include <score_test/App.hpp>
#include <score_test/Keyboard.hpp>

#include <QAbstractItemView>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QHideEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

#include <catch2/catch_all.hpp>

namespace
{
struct Scene final : public QGraphicsScene
{
  using QGraphicsScene::sendEvent;
};

void rightClick(Scene& scene, score::QGraphicsCombo& item)
{
  for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setButton(Qt::RightButton);
    ev.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::RightButton
                                                          : Qt::NoButton);
    ev.setScenePos({10., 10.});
    ev.setPos({1., 1.});
    scene.sendEvent(&item, &ev);
  }

  // The editor is built from the event loop so that it outlives the click.
  qApp->processEvents();
}

score::ComboBoxWithEnter* editorIn(Scene& scene)
{
  for(auto* it : scene.items())
    if(auto* proxy = qgraphicsitem_cast<QGraphicsProxyWidget*>(it))
      if(auto* cb = qobject_cast<score::ComboBoxWithEnter*>(proxy->widget()))
        return cb;
  return nullptr;
}
}

TEST_CASE("combo box opens a drop-down listing its entries")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);
    item.setValue(1);

    REQUIRE(editorIn(scene) == nullptr);

    rightClick(scene, item);

    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);
    CHECK(editor->count() == 3);
    CHECK(editor->itemText(0) == "a");
    CHECK(editor->itemText(2) == "c");
    CHECK(editor->currentIndex() == 1);
  });
}

TEST_CASE("picking an entry in the drop-down sets the value")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);

    int moved{}, released{};
    QObject::connect(&item, &score::QGraphicsCombo::sliderMoved, &item, [&] { moved++; });
    QObject::connect(
        &item, &score::QGraphicsCombo::sliderReleased, &item, [&] { released++; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    editor->setCurrentIndex(2);
    editor->activated(2);

    CHECK(item.value() == 2);
    CHECK(moved == 1);
    CHECK(released == 1);

    // ... and the editor goes away.
    qApp->processEvents();
    CHECK(editorIn(scene) == nullptr);
  });
}

TEST_CASE("a non-editable combo box refuses free text")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);

    QString edited;
    QObject::connect(
        &item, &score::QGraphicsCombo::valueEdited, &item,
        [&](const QString& t) { edited = t; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);
    CHECK_FALSE(editor->isEditable());

    editor->editingFinished();
    CHECK(edited.isEmpty());
    CHECK(item.value() == 0);
  });
}

TEST_CASE("an editable combo box reports a value that is not in the list")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);
    item.setEditable(true);

    QString edited;
    int moved{};
    QObject::connect(
        &item, &score::QGraphicsCombo::valueEdited, &item,
        [&](const QString& t) { edited = t; });
    QObject::connect(&item, &score::QGraphicsCombo::sliderMoved, &item, [&] { moved++; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);
    REQUIRE(editor->isEditable());

    editor->setCurrentText("zzz");
    editor->editingFinished();

    CHECK(edited == "zzz");
    // A free value is not one of the entries, so the displayed index stays put.
    CHECK(moved == 0);
    CHECK(item.value() == 0);
  });
}

TEST_CASE("an editable combo box still recognizes a listed value typed by hand")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);
    item.setEditable(true);

    QString edited;
    QObject::connect(
        &item, &score::QGraphicsCombo::valueEdited, &item,
        [&](const QString& t) { edited = t; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    editor->setCurrentText("b");
    editor->editingFinished();

    CHECK(edited.isEmpty());
    CHECK(item.value() == 1);
  });
}

// A single-entry enumeration used to divide by zero when dragged, and an empty
// one clamped against a negative upper bound.
TEST_CASE("degenerate combo boxes survive being dragged")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo one{QStringList{"only"}, nullptr};
    score::QGraphicsCombo none{QStringList{}, nullptr};
    scene.addItem(&one);
    scene.addItem(&none);

    score::InfiniteScroller::cancel();

    for(auto* item : {&one, &none})
    {
      for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove,
                       QEvent::GraphicsSceneMouseRelease})
      {
        QGraphicsSceneMouseEvent ev{type};
        ev.setButton(Qt::LeftButton);
        ev.setButtons(Qt::LeftButton);
        ev.setScreenPos({500, 500});
        ev.setLastScreenPos({500, 520});
        ev.setButtonDownScreenPos(Qt::LeftButton, {500, 520});
        scene.sendEvent(item, &ev);
      }
    }

    CHECK(one.value() == 0);
    CHECK(none.value() == 0);
  });
}

// A device may advertise an enumeration and then list nothing in it: that used
// to abort on an assertion.
TEST_CASE("an empty enumeration does not abort")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsEnum item{nullptr};
    scene.addItem(&item);

    item.setValue(3);
    CHECK(item.value() == 0);

    item.array = std::vector<QString>{"a", "b"};
    item.setValue(1);
    CHECK(item.value() == 1);
    item.setValue(99);
    CHECK(item.value() == 1);
  });
}

// Escape abandons an edit; the previous handler treated it exactly like Enter
// and committed. Driven through real key events, so that ComboBoxWithEnter's
// own handling is what is under test.
TEST_CASE("escape leaves the value alone")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);
    item.setEditable(true);

    QString edited;
    int moved{};
    QObject::connect(
        &item, &score::QGraphicsCombo::valueEdited, &item,
        [&](const QString& t) { edited = t; });
    QObject::connect(&item, &score::QGraphicsCombo::sliderMoved, &item, [&] { moved++; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);
    editor->hidePopup();
    editor->setCurrentText("zzz");

    score::test::keyClick(*editor, Qt::Key_Escape);

    CHECK(edited.isEmpty());
    CHECK(moved == 0);
    CHECK(item.value() == 0);
  });
}

TEST_CASE("enter takes what is in the box")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);
    item.setEditable(true);

    QString edited;
    QObject::connect(
        &item, &score::QGraphicsCombo::valueEdited, &item,
        [&](const QString& t) { edited = t; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);
    editor->hidePopup();
    editor->setCurrentText("zzz");

    score::test::keyClick(*editor, Qt::Key_Return);
    CHECK(edited == "zzz");
  });
}

// A control can hold a value that is not in the list, and then the item still
// paints entry 0: picking that entry has to be a real pick.
TEST_CASE("the displayed entry stays selectable")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);

    int moved{};
    QObject::connect(&item, &score::QGraphicsCombo::sliderMoved, &item, [&] { moved++; });

    rightClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    REQUIRE(item.value() == 0);
    editor->activated(0);
    CHECK(moved == 1);
  });
}

TEST_CASE("tab pages retain model selection before layout and through relayout")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    score::GraphicsTabLayout tabs{nullptr};
    auto* json = new QGraphicsRectItem{QRectF{0., 0., 100., 30.}, &tabs};
    auto* binary = new QGraphicsRectItem{QRectF{0., 0., 160., 80.}, &tabs};
    tabs.addTab("JSON");
    tabs.addTab("Binary");
    SECTION("visible tab selector")
    {
      tabs.setTabBarVisible(true);
    }
    SECTION("model-only panes")
    {
      tabs.setTabBarVisible(false);
    }

    tabs.setCurrentIndex(1);
    tabs.layout();
    CHECK_FALSE(json->isVisible());
    CHECK(binary->isVisible());

    tabs.layout();
    CHECK_FALSE(json->isVisible());
    CHECK(binary->isVisible());

    tabs.setCurrentIndex(0);
    CHECK(json->isVisible());
    CHECK_FALSE(binary->isVisible());
  });
}

// ---------------------------------------------------------------------------
// Clicking opens the drop-down. The editor is built from the event loop and
// lives in the scene, so the ways it can outlive what it points at are what
// these check: the combo box vanishing under it, the item list being replaced
// while it is open, a second click arriving before it appears.

namespace
{
void leftClick(Scene& scene, score::QGraphicsCombo& item, QPoint at = {500, 500})
{
  for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(type == QEvent::GraphicsSceneMousePress ? Qt::LeftButton
                                                          : Qt::NoButton);
    ev.setScreenPos(at);
    ev.setLastScreenPos(at);
    ev.setButtonDownScreenPos(Qt::LeftButton, at);
    ev.setScenePos({10., 10.});
    ev.setPos({1., 1.});
    scene.sendEvent(&item, &ev);
  }
  qApp->processEvents();
}

void leftDrag(Scene& scene, score::QGraphicsCombo& item)
{
  const QPoint from{500, 500};
  const QPoint to{500, 560};
  for(auto type : {QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove,
                   QEvent::GraphicsSceneMouseRelease})
  {
    QGraphicsSceneMouseEvent ev{type};
    ev.setButton(Qt::LeftButton);
    ev.setButtons(type == QEvent::GraphicsSceneMouseRelease ? Qt::NoButton
                                                            : Qt::LeftButton);
    ev.setScreenPos(type == QEvent::GraphicsSceneMousePress ? from : to);
    ev.setLastScreenPos(from);
    ev.setButtonDownScreenPos(Qt::LeftButton, from);
    scene.sendEvent(&item, &ev);
  }
  qApp->processEvents();
}
}

TEST_CASE("a click opens the drop-down, a drag does not")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);

    score::InfiniteScroller::cancel();
    leftDrag(scene, item);
    CHECK(editorIn(scene) == nullptr);

    score::InfiniteScroller::cancel();
    leftClick(scene, item);
    CHECK(editorIn(scene) != nullptr);
  });
}

TEST_CASE("clicking twice never leaves two drop-downs behind")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);

    // Both clicks land before the deferred build runs, and a right click is
    // thrown in because either button opens one.
    for(int i = 0; i < 3; i++)
    {
      QGraphicsSceneMouseEvent press{QEvent::GraphicsSceneMousePress};
      press.setButton(Qt::LeftButton);
      press.setButtons(Qt::LeftButton);
      press.setButtonDownScreenPos(Qt::LeftButton, {500, 500});
      press.setScreenPos({500, 500});
      scene.sendEvent(&item, &press);

      QGraphicsSceneMouseEvent rel{QEvent::GraphicsSceneMouseRelease};
      rel.setButton(Qt::LeftButton);
      rel.setButtons(Qt::NoButton);
      rel.setButtonDownScreenPos(Qt::LeftButton, {500, 500});
      rel.setScreenPos({500, 500});
      scene.sendEvent(&item, &rel);
    }
    rightClick(scene, item);
    qApp->processEvents();

    int editors = 0;
    for(auto* it : scene.items())
      if(auto* proxy = qgraphicsitem_cast<QGraphicsProxyWidget*>(it))
        if(qobject_cast<score::ComboBoxWithEnter*>(proxy->widget()))
          editors++;
    CHECK(editors == 1);
  });
}

// The runtime-populated case: an avnd object or a folder watcher can replace
// the items from under an open drop-down.
TEST_CASE("the drop-down survives its items being replaced under it")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);
    item.setValue(2);

    int moved{};
    QObject::connect(&item, &score::QGraphicsCombo::sliderMoved, &item, [&] { moved++; });

    leftClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    SECTION("picking an entry that is still there selects it by name")
    {
      // The list is reordered while the popup shows the old order.
      item.array = QStringList{"c", "b", "a"};
      editor->activated(0); // "a" in the editor's snapshot
      qApp->processEvents();
      CHECK(item.value() == 2); // ... which is now index 2
      CHECK(moved == 1);
    }

    SECTION("picking an entry that has since disappeared changes nothing")
    {
      item.array = QStringList{"x", "y"};
      editor->activated(2); // "c", gone from the list
      qApp->processEvents();
      CHECK(moved == 0);
      // The selection is left where it was rather than guessed at, exactly as
      // Process::ComboBox::setAlternatives leaves a value it cannot find. That
      // leaves the index pointing past the shorter list, which every reader
      // has to tolerate -- so paint it and see.
      CHECK(item.value() == 2);
      QImage img{32, 32, QImage::Format_ARGB32};
      QPainter p{&img};
      QStyleOptionGraphicsItem opt;
      item.paint(&p, &opt, nullptr);
      SUCCEED("painting past the end of the list is guarded");
    }

    SECTION("the list emptying entirely does not take the editor down with it")
    {
      item.array = QStringList{};
      editor->activated(1);
      qApp->processEvents();
      CHECK(moved == 0);
    }
  });
}

TEST_CASE("the combo box may be destroyed while its drop-down is open")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    auto* item = new score::QGraphicsCombo{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(item);

    leftClick(scene, *item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    // The process this control belongs to goes away while the popup is up.
    scene.removeItem(item);
    delete item;
    qApp->processEvents();

    // Whatever the user does next must not reach the dead combo box.
    if(auto* still = editorIn(scene))
    {
      still->activated(1);
      still->editingFinished();
    }
    qApp->processEvents();
    SUCCEED("no crash");
  });
}

TEST_CASE("a click on a degenerate combo box opens nothing and does not crash")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo none{QStringList{}, nullptr};
    score::QGraphicsCombo one{QStringList{"only"}, nullptr};
    scene.addItem(&none);
    scene.addItem(&one);

    score::InfiniteScroller::cancel();
    leftClick(scene, none);
    CHECK(editorIn(scene) == nullptr); // nothing to pick from
    CHECK(none.value() == 0);

    score::InfiniteScroller::cancel();
    leftClick(scene, one);
    CHECK(one.value() == 0);
  });
}

// The scene can take the implicit grab away without ever sending a release.
TEST_CASE("losing the mouse grab mid-drag does not turn into a click")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);
    score::InfiniteScroller::cancel();

    QGraphicsSceneMouseEvent press{QEvent::GraphicsSceneMousePress};
    press.setButton(Qt::LeftButton);
    press.setButtons(Qt::LeftButton);
    press.setButtonDownScreenPos(Qt::LeftButton, {500, 500});
    press.setScreenPos({500, 500});
    scene.sendEvent(&item, &press);

    QEvent ungrab{QEvent::UngrabMouse};
    scene.sendEvent(&item, &ungrab);
    qApp->processEvents();

    CHECK(editorIn(scene) == nullptr);
  });
}

// Dismissing the list by clicking away leaves a non-editable box with nothing
// to do. ComboBoxWithEnter only reports a focus change while the list is down,
// so without the popup watcher the collapsed editor stayed on the scene for
// good -- and clicking again put a second one next to it.
TEST_CASE("dismissing the drop-down takes the editor away with it")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b", "c"}, nullptr};
    scene.addItem(&item);

    leftClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    // What clicking outside the list does: the view hides, nothing is
    // activated. Sent directly, because an offscreen popup is never shown and
    // so would never hide either.
    QHideEvent hide;
    qApp->sendEvent(editor->view(), &hide);
    qApp->processEvents();
    qApp->processEvents();

    CHECK(editorIn(scene) == nullptr);
    CHECK(item.value() == 0);
  });
}

TEST_CASE("an editable combo box stays up when its list is dismissed")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    Scene scene;
    score::QGraphicsCombo item{QStringList{"a", "b"}, nullptr};
    scene.addItem(&item);
    item.setEditable(true);

    leftClick(scene, item);
    auto* editor = editorIn(scene);
    REQUIRE(editor != nullptr);

    // Closing the list to type instead must not take the box away.
    QHideEvent hide;
    qApp->sendEvent(editor->view(), &hide);
    qApp->processEvents();
    qApp->processEvents();

    CHECK(editorIn(scene) != nullptr);
  });
}
