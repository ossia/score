// score::QGraphicsCombo's drop-down editor: the popup that a right click puts
// in the scene, and what it does with a value that is not in the list.

#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/graphics/widgets/QGraphicsEnum.hpp>
#include <score/widgets/ComboBox.hpp>

#include <score_test/App.hpp>
#include <score_test/Keyboard.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

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
