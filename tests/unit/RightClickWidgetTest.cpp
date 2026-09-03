// The one type-in box a right-click may have open.
//
// The box is parented to the scene, not to the control that raised it, so
// nothing took the previous one down: right-clicking a second control left
// both floating over the scene. Tracking it in one place means the owner of a
// box and the code that closes it are two different things, which is how the
// double free this guards against happened.

#include <score_test/App.hpp>

#include <score/graphics/RightClickWidget.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QLabel>
#include <QPointer>

namespace
{
QGraphicsProxyWidget* raise(QGraphicsScene& scene)
{
  auto* proxy = scene.addWidget(new QLabel{"x"}, Qt::FramelessWindowHint);
  score::currentRightClickWidget() = proxy;
  return proxy;
}
}

TEST_CASE("one right-click box at a time", "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;
    REQUIRE(score::currentRightClickWidget() == nullptr);

    QPointer<QGraphicsProxyWidget> first = raise(scene);
    REQUIRE(first != nullptr);
    CHECK(scene.items().size() == 1);

    // Raising a second takes the first away, whichever control raised it.
    score::closeRightClickWidget();
    QPointer<QGraphicsProxyWidget> second = raise(scene);

    CHECK(first == nullptr);
    CHECK(second != nullptr);
    CHECK(scene.items().size() == 1);

    score::closeRightClickWidget();
    CHECK(second == nullptr);
    CHECK(scene.items().isEmpty());
  });
}

// A box torn down by the control that raised it must leave nothing behind
// here: closeRightClickWidget() would otherwise free it a second time, which
// is what the queued teardown in DefaultGraphicsSliderImpl used to do.
TEST_CASE("a box deleted by its own control leaves no dangling pointer",
          "[graphics]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    QGraphicsScene scene;

    auto* proxy = raise(scene);
    REQUIRE(score::currentRightClickWidget() == proxy);

    // What the control's own teardown does.
    scene.removeItem(proxy);
    delete proxy;

    // The QPointer has already emptied itself, so closing is a no-op rather
    // than a second delete.
    CHECK(score::currentRightClickWidget() == nullptr);
    score::closeRightClickWidget();
    CHECK(score::currentRightClickWidget() == nullptr);
  });
}
