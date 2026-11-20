// Each widget and window gets the cursors of the screen it is on, on a
// desktop whose screens have different scales, the primary one included.
//
// Runs on Qt's offscreen platform, configured with several screens scaled as
// Windows scales them, through their logical DPI: the theme takes it for the
// Windows plug-in (CursorPlatform::Windows), where a pixmap whose ratio is not
// the factor of its window's screen is resampled (qwindowscursor.cpp
// createPixmapCursor). X11 has one pointer scale for every screen, so the
// tests' Xvfb cannot show this; what Windows and macOS draw is checked on
// those systems.
#include <score/widgets/CursorTheme.hpp>

#include <QApplication>
#include <QDir>
#include <QDrag>
#include <QFile>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QScreen>
#include <QTemporaryDir>
#include <QWidget>
#include <QWindow>
#include <qpa/qplatformnativeinterface.h>
#include <qpa/qwindowsysteminterface.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <string_view>

using score::CursorArtwork;

namespace
{
//! A screen scaled the way Windows scales one: through its logical DPI,
//! which Qt turns into the screen's factor, that of its windows too.
QJsonObject screen(const char* name, int x, double scale)
{
  return QJsonObject{
      {"name", name},
      {"x", x},
      {"y", 0},
      // Native pixels: big enough for the windows at every scale.
      {"width", 4000},
      {"height", 3000},
      {"logicalDpi", int(std::lround(96 * scale))},
      {"logicalBaseDpi", 96},
      {"dpr", 1.}};
}

// The primary first, at a scale none of the others has.
const std::pair<const char*, double> screens[]
    = {{"primary", 1.75}, {"s1", 1.},  {"s125", 1.25},
       {"s15", 1.5},      {"s2", 2.},  {"s3", 3.}};

QJsonObject configuration(bool withExtra, double s2 = 2.)
{
  QJsonArray list;
  int x = 0;
  for(auto [name, dpr] : screens)
  {
    list.append(screen(name, x, std::string_view{name} == "s2" ? s2 : dpr));
    x += 4000;
  }
  if(withExtra)
    list.append(screen("added", x, 2.5));
  return QJsonObject{{"synchronousWindowSystemEvents", true}, {"screens", list}};
}

QScreen* screenNamed(const QString& name)
{
  for(QScreen* s : QGuiApplication::screens())
    if(s->name() == name)
      return s;
  return nullptr;
}

//! What qwindowscursor.cpp does with the pixmap of a cursor on a window of
//! that screen: rescales it unless its ratio is the screen's factor.
bool resampledOn(const QCursor& c, const QScreen& s)
{
  const double pixmapScaleFactor = s.devicePixelRatio() / c.pixmap().devicePixelRatio();
  return !qFuzzyCompare(pixmapScaleFactor, 1.);
}

void checkCursorFor(const QCursor& c, const QScreen& s, const char* what)
{
  INFO(what);
  CAPTURE(s.name().toStdString(), s.devicePixelRatio(), c.pixmap().devicePixelRatio(),
          c.pixmap().width());
  REQUIRE(c.shape() == Qt::BitmapCursor);
  CHECK_FALSE(resampledOn(c, s));
  CHECK(c.pixmap().width() == std::lround(32 * s.devicePixelRatio()));
}

void settle()
{
  for(int i = 0; i < 5; i++)
    QCoreApplication::processEvents();
}

void moveTo(QWindow* win, QPoint local)
{
  QCursor::setPos(win->mapToGlobal(local));
  QWindowSystemInterface::handleMouseEvent(
      win, QPointF(local), QPointF(win->mapToGlobal(local)), Qt::NoButton, Qt::NoButton,
      QEvent::MouseMove);
  QWindowSystemInterface::flushWindowSystemEvents();
  settle();
}
}

TEST_CASE("Each widget and window gets the cursors of its screen", "[cursor]")
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString config = dir.filePath("screens.json");
  {
    QFile f{config};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument{configuration(false)}.toJson());
  }
  // Qt splits the platform's arguments on ':', which would cut a Windows path
  // at its drive letter: the file is given relative to the current directory.
  QDir::setCurrent(dir.path());
  const QByteArray platform = "offscreen:configfile=screens.json";
  int argc = 3;
  QByteArray arg0 = "test", arg1 = "-platform", arg2 = platform;
  char* argv[] = {arg0.data(), arg1.data(), arg2.data(), nullptr};
  QApplication app{argc, argv};
  REQUIRE(QGuiApplication::screens().size() == int(std::size(screens)));
  REQUIRE(QGuiApplication::primaryScreen()->devicePixelRatio() == 1.75);

  auto& theme = score::CursorTheme::instance();
  theme.install();
  REQUIRE(theme.installed());
  REQUIRE(theme.platform() == score::CursorPlatform::Windows);

  // The cursors score's code sets are the primary screen's.
  CHECK(theme.screenScale() == Catch::Approx(1.75));
  checkCursorFor(theme.cursor(CursorArtwork::Pointer), *QGuiApplication::primaryScreen(), "primary pointer");

  // A window on each screen: its pointer, a child with one of the primary's
  // cursors, a child with a standard shape, a graphics item with one of the
  // primary's cursors.
  struct OnScreen
  {
    std::unique_ptr<QWidget> top;
    QWidget* fromPrimary{};
    QWidget* standard{};
    QGraphicsView* view{};
    QGraphicsRectItem* item{};
  };
  std::vector<OnScreen> windows;
  for(auto [name, dpr] : screens)
  {
    QScreen* s = screenNamed(name);
    REQUIRE(s);
    OnScreen w;
    w.top = std::make_unique<QWidget>();
    w.top->setGeometry(QRect{s->geometry().topLeft() + QPoint{100, 100}, QSize{400, 300}});
    w.fromPrimary = new QWidget{w.top.get()};
    w.fromPrimary->setGeometry(0, 0, 100, 100);
    w.fromPrimary->setCursor(theme.cursor(CursorArtwork::IBeam));
    w.standard = new QWidget{w.top.get()};
    w.standard->setGeometry(100, 0, 100, 100);
    w.standard->setCursor(Qt::SizeAllCursor);
    auto* scene = new QGraphicsScene{w.top.get()};
    w.item = scene->addRect(0, 0, 50, 50);
    w.item->setCursor(theme.cursor(CursorArtwork::OpenHand));
    w.view = new QGraphicsView{scene, w.top.get()};
    w.view->setGeometry(0, 100, 300, 200);
    w.view->setSceneRect(0, 0, 50, 50);
    w.top->show();
    windows.push_back(std::move(w));
  }
  settle();

  for(auto& w : windows)
  {
    QWindow* win = w.top->windowHandle();
    REQUIRE(win);
    const QScreen& s = *win->screen();
    CAPTURE(s.name().toStdString());
    checkCursorFor(w.top->cursor(), s, "w.top->cursor()");
    checkCursorFor(w.fromPrimary->cursor(), s, "w.fromPrimary->cursor()");
    checkCursorFor(w.standard->cursor(), s, "w.standard->cursor()");
    checkCursorFor(win->cursor(), s, "win->cursor()");
    CHECK(theme.screenScaleOf(*w.top) == Catch::Approx(s.devicePixelRatio()));

    // Over the item: QGraphicsView gives its viewport the item's cursor,
    // which the theme swaps for this screen's, the item's own included so
    // that the next moves do not change it again.
    const QPoint onItem
        = w.view->viewport()->mapTo(w.top.get(), w.view->mapFromScene(QPointF{25, 25}));
    moveTo(win, onItem);
    checkCursorFor(w.view->viewport()->cursor(), s, "w.view->viewport()->cursor()");
    checkCursorFor(w.item->cursor(), s, "w.item->cursor()");
    const qint64 key = w.view->viewport()->cursor().pixmap().cacheKey();
    moveTo(win, onItem + QPoint{1, 1});
    CHECK(w.view->viewport()->cursor().pixmap().cacheKey() == key);
  }

  // One set per scale: windows of screens of the same scale share their
  // cursors.
  CHECK(&theme.cursor(CursorArtwork::Pointer, 3.) == &theme.cursor(CursorArtwork::Pointer, 3.));

  // A window moved to a screen of another scale while it is shown.
  {
    auto& w = windows[1]; // on the screen of scale 1
    QScreen* to = screenNamed("s3");
    w.top->move(to->geometry().topLeft() + QPoint{200, 200});
    settle();
    REQUIRE(w.top->windowHandle()->screen() == to);
    checkCursorFor(w.top->cursor(), *to, "w.top->cursor()");
    checkCursorFor(w.fromPrimary->cursor(), *to, "w.fromPrimary->cursor()");
    checkCursorFor(w.standard->cursor(), *to, "w.standard->cursor()");
    checkCursorFor(w.top->windowHandle()->cursor(), *to, "w.top->windowHandle()->cursor()");
  }

  // The override cursor gets the set of the screen under the pointer.
  {
    QWindow* on3 = windows[5].top->windowHandle();
    moveTo(on3, {10, 250});
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    QCoreApplication::postEvent(&app, new QEvent{QEvent::User});
    settle();
    REQUIRE(QGuiApplication::overrideCursor());
    checkCursorFor(*QGuiApplication::overrideCursor(), *on3->screen(), "*QGuiApplication::overrideCursor()");

    QWindow* on125 = windows[2].top->windowHandle();
    moveTo(on125, {10, 250});
    REQUIRE(QGuiApplication::overrideCursor());
    checkCursorFor(*QGuiApplication::overrideCursor(), *on125->screen(), "*QGuiApplication::overrideCursor()");
    QGuiApplication::restoreOverrideCursor();
  }

  // The cursors of a drag: for the screen the pointer is on when it starts,
  // and again when it enters a window on another one.
  {
    QWindow* on3 = windows[5].top->windowHandle();
    moveTo(on3, {10, 250});
    QDrag drag{windows[5].top.get()};
    drag.setMimeData(new QMimeData);
    theme.setDragCursors(drag);
    for(auto action : {Qt::CopyAction, Qt::MoveAction, Qt::LinkAction, Qt::IgnoreAction})
    {
      CAPTURE(int(action));
      const QPixmap pm = drag.dragCursor(action);
      REQUIRE_FALSE(pm.isNull());
      CHECK(pm.cacheKey() == theme.dragCursor(action, 3.).cacheKey());
      // QBasicDrag makes a QCursor of it: shown unscaled on that screen.
      CHECK_FALSE(resampledOn(QCursor{pm}, *on3->screen()));
    }

    QWindow* on2 = windows[4].top->windowHandle();
    moveTo(on2, {10, 250});
    theme.setDragCursors(drag);
    CHECK(drag.dragCursor(Qt::MoveAction).cacheKey()
          == theme.dragCursor(Qt::MoveAction, 2.).cacheKey());

    // A cursor the drag was given by its code stays.
    QPixmap own{8, 8};
    own.fill(Qt::red);
    drag.setDragCursor(own, Qt::CopyAction);
    moveTo(on3, {10, 250});
    theme.setDragCursors(drag);
    CHECK(drag.dragCursor(Qt::CopyAction).cacheKey() == own.cacheKey());
  }

  // A screen plugged in: a window moved to it gets cursors of its scale.
  {
    using SetConfiguration = void (*)(const QJsonObject&, QPlatformNativeInterface*);
    auto* native = QGuiApplication::platformNativeInterface();
    REQUIRE(native);
    auto set = reinterpret_cast<SetConfiguration>(
        native->nativeResourceForIntegration("setConfiguration"));
    REQUIRE(set);
    set(configuration(true), native);
    settle();
    QScreen* added = screenNamed("added");
    REQUIRE(added);
    REQUIRE(added->devicePixelRatio() == 2.5);
    auto& w = windows[0];
    w.top->move(added->geometry().topLeft() + QPoint{100, 100});
    settle();
    REQUIRE(w.top->windowHandle()->screen() == added);
    checkCursorFor(w.top->cursor(), *added, "w.top->cursor()");
    checkCursorFor(w.fromPrimary->cursor(), *added, "w.fromPrimary->cursor()");
  }

  // The scale of a screen changed in the display settings: its windows get
  // cursors of the new scale.
  {
    using SetConfiguration = void (*)(const QJsonObject&, QPlatformNativeInterface*);
    auto* native = QGuiApplication::platformNativeInterface();
    auto set = reinterpret_cast<SetConfiguration>(
        native->nativeResourceForIntegration("setConfiguration"));
    set(configuration(true, 2.25), native);
    settle();
    QScreen* s2 = screenNamed("s2");
    REQUIRE(s2->devicePixelRatio() == 2.25);
    auto& w = windows[4];
    REQUIRE(w.top->windowHandle()->screen() == s2);
    checkCursorFor(w.top->cursor(), *s2, "w.top->cursor()");
    checkCursorFor(w.fromPrimary->cursor(), *s2, "w.fromPrimary->cursor()");
    checkCursorFor(w.standard->cursor(), *s2, "w.standard->cursor()");
  }

  windows.clear();
}
