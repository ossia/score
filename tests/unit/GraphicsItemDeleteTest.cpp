// An item destroyed from inside its own event delivery corrupts the scene's
// mouse-grabber stack.
//
// QGraphicsScenePrivate::ungrabMouse sends QEvent::UngrabMouse to the item and
// only afterwards does mouseGrabberItems.takeLast(). An item that removes
// itself from the scene in that handler is popped once by removeItemHelper, so
// the pending takeLast() pops again -- taking the grab of whoever is below it
// on the stack, or running on an empty list.
//
// The stack is exercised with two grabbers so the damage is *observable*
// instead of merely undefined: the item below must still hold its grab
// afterwards. With one grabber the second pop would be a QList::takeLast() on
// an empty list, which only asserts in a debug-built Qt and is otherwise silent
// corruption -- not something to hang a regression test on.

#include <score/graphics/GraphicsItem.hpp>

#include <score_test/App.hpp>

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPointer>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct PlainItem : public QGraphicsItem
{
  QRectF boundingRect() const override { return {0., 0., 10., 10.}; }
  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override { }
};

//! Deletes itself the moment the scene takes its mouse grab away, which is what
//! a control does when the edit it closes there tears its own UI down.
struct SelfDeletingItem final : public PlainItem
{
  bool* destroyed{};
  bool deleting{};

  ~SelfDeletingItem()
  {
    if(destroyed)
      *destroyed = true;
  }

  bool sceneEvent(QEvent* event) override
  {
    // removeItem() re-enters ungrabMouse() and sends a second UngrabMouse, so
    // only the first one may act.
    if(event->type() == QEvent::UngrabMouse && !deleting)
    {
      deleting = true;
      deleteGraphicsItem(this);
      // Nothing may touch `this` past here when the deletion was not deferred:
      // that use-after-free is the other half of the bug, and letting it fire
      // would just crash the run before the grabber stack can be inspected.
      return true;
    }
    return QGraphicsItem::sceneEvent(event);
  }
};

//! Same, as a QGraphicsObject, for the deleteGraphicsObject() overload.
struct SelfDeletingObject final : public QGraphicsObject
{
  bool deleting{};

  QRectF boundingRect() const override { return {0., 0., 10., 10.}; }
  void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override { }

  bool sceneEvent(QEvent* event) override
  {
    if(event->type() == QEvent::UngrabMouse && !deleting)
    {
      deleting = true;
      deleteGraphicsObject(this);
      return true;
    }
    return QGraphicsObject::sceneEvent(event);
  }
};
}

TEST_CASE(
    "deleting an item during its own ungrab leaves the grabber stack alone",
    "[graphics][lifetime]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;

    auto* keeper = new PlainItem;
    auto* victim = new SelfDeletingItem;
    bool victimDestroyed = false;
    victim->destroyed = &victimDestroyed;
    scene.addItem(keeper);
    scene.addItem(victim);

    // Two grabbers: keeper underneath, victim on top. Both explicit, so taking
    // the second does not drop the first.
    keeper->grabMouse();
    victim->grabMouse();
    REQUIRE(scene.mouseGrabberItem() == victim);

    // The scene takes victim's grab away; victim deletes itself in the handler.
    victim->ungrabMouse();

    // Without the deferral, removeItemHelper pops victim and the pending
    // takeLast() in the outer ungrabMouse() frame then pops keeper -- keeper
    // silently loses a grab nobody released.
    CHECK(scene.mouseGrabberItem() == keeper);

    // ... and the deletion still happens, just one event-loop pass later.
    CHECK(!victimDestroyed);
    QCoreApplication::processEvents();
    CHECK(victimDestroyed);

    keeper->ungrabMouse();
    scene.removeItem(keeper);
    delete keeper;
  });
}

TEST_CASE(
    "the QGraphicsObject overload defers the same way", "[graphics][lifetime]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;

    auto* keeper = new PlainItem;
    auto* victim = new SelfDeletingObject;
    QPointer<QGraphicsObject> alive{victim};
    scene.addItem(keeper);
    scene.addItem(victim);

    keeper->grabMouse();
    victim->grabMouse();
    REQUIRE(scene.mouseGrabberItem() == victim);

    victim->ungrabMouse();

    CHECK(scene.mouseGrabberItem() == keeper);
    CHECK(!alive.isNull());

    // Two stages here, unlike the plain-item case: processEvents() runs the
    // deferred deleteGraphicsObject(), which then calls deleteLater() and posts
    // a DeferredDelete of its own.
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(alive.isNull());

    keeper->ungrabMouse();
    scene.removeItem(keeper);
    delete keeper;
  });
}
