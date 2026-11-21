// Every cursor score shows is its own artwork: no standard shape reaches the
// platform, whether score's code sets it or one of Qt's widgets does.
//
// Runs on the real platform plug-in (xcb on the tests' private Xvfb), with
// the pointer driven through QWindowSystemInterface, the way the platform
// plug-in delivers it. Out of its reach: the native dialogs (file, colour)
// of the platform, the editors of audio plug-ins, the windows of the GPU
// outputs, and the cursors the system draws during drag and drop on macOS
// and Wayland. It checks what Qt hands the platform, not what the X server
// shows, which takes reading the cursor back from the server with XFixes.
#include <Process/Process.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/model/Skin.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <score/widgets/CursorTheme.hpp>

#include <ossia/detail/algorithms.hpp>

#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>
#include <core/view/Window.hpp>

#include <QApplication>
#include <QComboBox>
#if QT_CONFIG(dockwidget)
#include <QDockWidget>
#endif
#include <QDrag>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#if QT_CONFIG(mdiarea)
#include <QMdiArea>
#include <QMdiSubWindow>
#endif
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#if QT_CONFIG(textbrowser)
#include <QTextBrowser>
#endif
#include <QTextEdit>
#include <QTimer>
#include <QTreeWidget>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#if __has_include(<QQuickWindow>) && defined(SCORE_CURSOR_TEST_HAS_QUICK)
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#define SCORE_CURSOR_TEST_QUICK 1
#else
#define SCORE_CURSOR_TEST_QUICK 0
#endif

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>
#include <score_test/Project.hpp>

#include <iostream>
#include <map>

#if defined(SCORE_CURSOR_TEST_MAC)
namespace score_test
{
bool macApplicationActive();
//! The size in points and the hotspot of the cursor the system shows.
bool macSystemCursor(QSizeF& points, QPointF& hotspot);
}
#endif

namespace
{
bool isScoreCursor(const QCursor& c)
{
  return !score::isThemeCursorShape(c.shape());
}

QString describe(const QObject* o)
{
  if(!o)
    return QStringLiteral("(nothing)");
  QString s = QString::fromLatin1(o->metaObject()->className());
  if(!o->objectName().isEmpty())
    s += QStringLiteral("[%1]").arg(o->objectName());
  if(o->parent())
    s += QStringLiteral(" in %1").arg(QString::fromLatin1(o->parent()->metaObject()->className()));
  return s;
}

//! The widgets that keep the standard shape (see CursorTheme): their window
//! shows score's.
bool keepsStandardShape(const QWidget& w)
{
  const auto shape = w.cursor().shape();
  if(shape == Qt::PointingHandCursor)
    if(auto* edit = qobject_cast<QTextEdit*>(w.parentWidget()))
      return edit->viewport() == &w;
  if(shape == Qt::SplitHCursor || shape == Qt::SplitVCursor)
    return qobject_cast<const QMainWindow*>(&w);
  return false;
}

//! Counts the times a window was given a standard shape, before the theme
//! replaced it: the instants a theme cursor reached the platform.
struct FlashCounter final : QObject
{
  std::map<std::string, int> flashes;
  //! The widget the pointer entered last, which the flash is put down to.
  QPointer<QObject> entered;
  bool eventFilter(QObject* o, QEvent* e) override
  {
    if(e->type() == QEvent::Enter && o->isWidgetType())
      entered = o;
    if(e->type() == QEvent::CursorChange && o->isWindowType())
    {
      const auto shape = static_cast<QWindow*>(o)->cursor().shape();
      if(score::isThemeCursorShape(shape))
        flashes[std::string(o->metaObject()->className()) + " shape "
                + std::to_string(int(shape)) + " over "
                + describe(entered.data()).toStdString()]++;
    }
    return false;
  }
  int total() const
  {
    int n = 0;
    for(auto& [k, v] : flashes)
      n += v;
    return n;
  }
};

void move(QWindow* win, QPoint local, Qt::MouseButtons buttons = Qt::NoButton)
{
  QWindowSystemInterface::handleMouseEvent(
      win, QPointF(local), QPointF(win->mapToGlobal(local)), buttons, Qt::NoButton,
      QEvent::MouseMove);
  QWindowSystemInterface::flushWindowSystemEvents();
}

void button(QWindow* win, QPoint local, QEvent::Type type)
{
  const auto buttons = type == QEvent::MouseButtonPress ? Qt::LeftButton : Qt::NoButton;
  QWindowSystemInterface::handleMouseEvent(
      win, QPointF(local), QPointF(win->mapToGlobal(local)), buttons, Qt::LeftButton,
      type);
  QWindowSystemInterface::flushWindowSystemEvents();
}

//! What is not score's in the state the application is in now.
QStringList audit()
{
  QStringList bad;
  for(QWidget* w : QApplication::allWidgets())
  {
    if(!w->isVisible())
      continue;
    if(w->isWindow() && !w->testAttribute(Qt::WA_SetCursor))
      bad << QStringLiteral("window without cursor: %1").arg(describe(w));
    if(w->testAttribute(Qt::WA_SetCursor) && !isScoreCursor(w->cursor())
       && !keepsStandardShape(*w))
      bad << QStringLiteral("widget %1: shape %2").arg(describe(w)).arg(w->cursor().shape());
  }
  for(QWindow* w : QGuiApplication::allWindows())
    if(w->isVisible() && !isScoreCursor(w->cursor()))
      bad << QStringLiteral("window %1: shape %2").arg(describe(w)).arg(w->cursor().shape());
  if(auto* c = QGuiApplication::overrideCursor(); c && !isScoreCursor(*c))
    bad << QStringLiteral("override cursor: shape %1").arg(c->shape());
  return bad;
}

//! Moves the pointer over the whole of a top-level widget, and checks the
//! cursor its window shows at every step.
QStringList sweep(QWidget& top, int step = 24)
{
  QStringList bad;
  QWindow* win = top.windowHandle();
  if(!win)
    return {QStringLiteral("no window for %1").arg(describe(&top))};
  for(int y = 1; y < top.height(); y += step)
  {
    for(int x = 1; x < top.width(); x += step)
    {
      move(win, {x, y});
      if(!isScoreCursor(win->cursor()))
        bad << QStringLiteral("(%1, %2) over %3: shape %4")
                   .arg(x)
                   .arg(y)
                   .arg(describe(top.childAt(x, y)))
                   .arg(win->cursor().shape());
    }
  }
  move(win, {-5, -5});
  return bad;
}

void report(const char* what, const QStringList& bad)
{
  INFO(what << ": " << bad.join(QStringLiteral("\n")).toStdString());
  CHECK(bad.isEmpty());
}

QPoint centre(QWidget& w, QWidget& top)
{
  return w.mapTo(&top, w.rect().center());
}

//! What the system itself shows over a popup, read back from AppKit: Qt's
//! popups never become key, and AppKit keeps its arrow over them unless score
//! sets the cursor itself. Elsewhere, what Qt hands the platform is checked
//! above.
void checkShownBySystem(QWidget& popup, const char* what)
{
#if defined(SCORE_CURSOR_TEST_MAC)
  if(!score_test::macApplicationActive())
  {
    WARN(what << ": score is not the active application, AppKit shows no cursor of its own");
    return;
  }
  QWindow* win = popup.windowHandle();
  REQUIRE(win);
  move(win, {popup.width() / 2, popup.height() / 2});
  QSizeF points;
  QPointF hotspot;
  REQUIRE(score_test::macSystemCursor(points, hotspot));
  const QCursor& c = win->cursor();
  REQUIRE(c.shape() == Qt::BitmapCursor);
  const QSizeF expected = QSizeF(c.pixmap().size()) / c.pixmap().devicePixelRatio();
  INFO(what << ": AppKit shows " << points.width() << "x" << points.height() << " pt, hotspot "
            << hotspot.x() << "," << hotspot.y());
  CHECK(points == expected);
  CHECK(hotspot == QPointF(c.hotSpot()));
#else
  (void)popup;
  (void)what;
#endif
}

struct DropTarget final : QWidget
{
  DropTarget() { setAcceptDrops(true); }
  void dragEnterEvent(QDragEnterEvent* e) override { e->acceptProposedAction(); }
  void dragMoveEvent(QDragMoveEvent* e) override { e->acceptProposedAction(); }
  void dropEvent(QDropEvent* e) override { e->acceptProposedAction(); }
};
}

TEST_CASE("No standard cursor reaches the platform", "[integration][cursor]")
{
  // The tests' X server has no pointer size: 24, the size the artwork is
  // drawn next to, so that the cursors measure their canvas at a zoom of 1.
  qputenv("XCURSOR_SIZE", "24");
  qputenv("SCORE_DESKTOP_XCURSOR_SIZE", "24");

  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto& theme = score::CursorTheme::instance();
    REQUIRE(theme.installed());

    FlashCounter flashes;
    qApp->installEventFilter(&flashes);

    // Every artwork renders, with its outline and its fill. Set
    // SCORE_CURSOR_DUMP_DIR to look at them.
    const QString dump = qEnvironmentVariable("SCORE_CURSOR_DUMP_DIR");
    for(int a = 0; a < int(score::CursorArtwork::Count); a++)
    {
      CAPTURE(a);
      const QCursor& c = theme.cursor(score::CursorArtwork(a));
      REQUIRE(c.shape() == Qt::BitmapCursor);
      const QImage img = c.pixmap().toImage().convertToFormat(QImage::Format_ARGB32);
      if(!dump.isEmpty())
        img.save(QStringLiteral("%1/%2.png").arg(dump).arg(a, 2, 10, QChar('0')));
      int dark = 0, light = 0;
      for(int y = 0; y < img.height(); y++)
        for(int x = 0; x < img.width(); x++)
        {
          const QColor px = img.pixelColor(x, y);
          if(px.alpha() > 200 && px.lightness() < 60)
            dark++;
          else if(px.alpha() > 200 && px.lightness() > 200)
            light++;
        }
      CHECK(dark > 10);
      CHECK(light > 10);
    }
    if(QGuiApplication::platformName() == QLatin1String("xcb"))
    {
      // Drawn at the canvas size times the zoom times the pointer size.
      CHECK(theme.cursor(score::CursorArtwork::Pointer).pixmap().size() == QSize(32, 32));
      // On the apex of the arrow, (10.5, 7.5) on the canvas.
      CHECK(theme.cursor(score::CursorArtwork::Pointer).hotSpot() == QPoint(10, 7));
    }

    // The skin hands out the theme's cursors.
    auto& skin = score::Skin::instance();
    CHECK(skin.CursorPointer.pixmap().cacheKey()
          == theme.cursor(score::CursorArtwork::Pointer).pixmap().cacheKey());
    CHECK(skin.cursor(Qt::CrossCursor).shape() == Qt::BitmapCursor);
    CHECK(skin.cursor(Qt::BlankCursor).shape() == Qt::BlankCursor);

    // The main window, a document with a scenario holding a curve and a
    // piano roll, and the panels: inspector, library, device explorer.
    score::View* view{};
    for(QWidget* w : QApplication::topLevelWidgets())
      if(auto* v = qobject_cast<score::View*>(w))
        view = v;
    REQUIRE(view);
    view->resize(1400, 900);
    view->show();
    score::test::settle();

    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    score::test::add_process(
        *doc, QStringLiteral("d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"), {});
    score::test::add_process(
        *doc, QStringLiteral("c189e507-3afa-4a53-9369-e38860e6bb2d"), {});
    score::test::settle();

    report("after start", audit());
    report("main window", sweep(*view, 16));
    report("main window, after the sweep", audit());

    // The settings dialog, each of its pages.
    {
      auto* dialog = new score::SettingsView<score::SettingsDelegateModel>{nullptr};
      auto* presenter
          = new score::SettingsPresenter<score::SettingsDelegateModel>{dialog, dialog};
      QSettings s;
      std::vector<std::unique_ptr<score::SettingsDelegateModel>> models;
      for(auto& f : ctx.interfaces<score::SettingsDelegateFactoryList>())
      {
        auto m = f.makeModel(s, ctx);
        if(!m)
          continue;
        if(auto* v = f.makeView())
        {
          if(auto* p = f.makePresenter(*m, *v, presenter))
          {
            presenter->addSettingsPresenter(p);
            dialog->addSettingsView(v);
          }
          else
          {
            delete v;
          }
        }
        models.push_back(std::move(m));
      }
      dialog->resize(1100, 700);
      dialog->show();
      score::test::settle();
      if(auto* stack = dialog->findChild<QStackedWidget*>())
      {
        for(int i = 0; i < stack->count(); i++)
        {
          stack->setCurrentIndex(i);
          score::test::settle(2);
          report("settings page", sweep(*dialog, 30));
        }
      }
      report("settings dialog", audit());
      delete dialog;
      models.clear();
      score::test::settle();
    }

    // Qt's widgets, which set cursors of their own.
    QMainWindow gallery;
    gallery.setObjectName("gallery");
    auto* split = new QSplitter;
    split->setObjectName("splitter");

    auto* tree = new QTreeWidget;
    tree->setColumnCount(3);
    tree->setHeaderLabels({"a", "b", "c"});
    tree->header()->setSectionResizeMode(QHeaderView::Interactive);
    for(int i = 0; i < 5; i++)
      new QTreeWidgetItem(tree, QStringList{"x", "y", "z"});
    split->addWidget(tree);

    auto* column = new QWidget;
    auto* lay = new QVBoxLayout{column};
    auto* line = new QLineEdit{"line edit"};
    auto* disabledLine = new QLineEdit{"disabled"};
    disabledLine->setEnabled(false);
    auto* readOnlyLine = new QLineEdit{"read only"};
    readOnlyLine->setReadOnly(true);
    auto* spin = new QSpinBox;
    auto* combo = new QComboBox;
    combo->setEditable(true);
    combo->addItems({"one", "two", "three"});
    auto* label = new QLabel{"<a href=\"https://ossia.io\">a link</a>"};
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    auto* tabs = new QTabBar;
    tabs->addTab("one");
    tabs->addTab("two");
    tabs->setTabsClosable(true);
    tabs->setMovable(true);
    auto* drop = new DropTarget;
    drop->setObjectName("drop target");
    drop->setMinimumHeight(40);
    for(QWidget* w :
        std::initializer_list<QWidget*>{line, disabledLine, readOnlyLine, spin, combo, label, tabs, drop})
      lay->addWidget(w);
    split->addWidget(column);

    auto* edit = new QTextEdit{"text edit"};
#if QT_CONFIG(textbrowser)
    auto* browser = new QTextBrowser;
    browser->setHtml("<p>before <a href=\"#anchor\">a link in a browser</a> after</p>");
#endif
    auto* plain = new QPlainTextEdit{"plain text edit"};
    auto* texts = new QSplitter{Qt::Vertical};
    texts->addWidget(edit);
#if QT_CONFIG(textbrowser)
    texts->addWidget(browser);
#endif
    texts->addWidget(plain);
    split->addWidget(texts);

    QGraphicsScene scene;
    auto* rect = scene.addRect(0, 0, 80, 80);
    rect->setCursor(Qt::SizeAllCursor);
    auto* gview = new QGraphicsView{&scene};
    gview->setDragMode(QGraphicsView::ScrollHandDrag);
    gview->setSceneRect(-200, -200, 600, 600);
    split->addWidget(gview);

#if QT_CONFIG(mdiarea)
    auto* mdi = new QMdiArea;
    auto* sub = mdi->addSubWindow(new QLabel{"mdi"});
    sub->setGeometry(10, 10, 140, 100);
    split->addWidget(mdi);
#endif

    gallery.setCentralWidget(split);
#if QT_CONFIG(dockwidget)
    auto* dockA = new QDockWidget{"dock a"};
    dockA->setWidget(new QLabel{"a"});
    auto* dockB = new QDockWidget{"dock b"};
    dockB->setWidget(new QLabel{"b"});
    gallery.addDockWidget(Qt::LeftDockWidgetArea, dockA);
    gallery.addDockWidget(Qt::LeftDockWidgetArea, dockB);
#endif
    gallery.statusBar()->setSizeGripEnabled(true);
    gallery.resize(1300, 600);
    gallery.move(20, 20);
    gallery.show();
    score::test::settle();

    report("gallery after show", audit());
    report("gallery", sweep(gallery, 8));
    QWindow* gwin = gallery.windowHandle();

    // Hover where Qt sets its cursors, and press where it changes them.
    {
      // The boundary between the header's first two sections: SplitH.
      auto* header = tree->header();
      const QPoint edge = header->mapTo(
          &gallery, QPoint{header->sectionViewportPosition(1), header->height() / 2});
      move(gwin, edge);
      CHECK(header->cursor().shape() == Qt::BitmapCursor);
      CHECK(isScoreCursor(gwin->cursor()));

      // The graphics item's cursor, and the view's hand while it scrolls.
      const QPoint onItem = gview->viewport()->mapTo(
          &gallery, gview->mapFromScene(rect->sceneBoundingRect().center()));
      move(gwin, onItem);
      CHECK(isScoreCursor(gwin->cursor()));
      const QPoint offItem = gview->viewport()->mapTo(&gallery, QPoint{4, 4});
      move(gwin, offItem);
      button(gwin, offItem, QEvent::MouseButtonPress);
      CHECK(isScoreCursor(gwin->cursor()));
      move(gwin, offItem + QPoint{20, 20}, Qt::LeftButton);
      CHECK(isScoreCursor(gwin->cursor()));
      button(gwin, offItem + QPoint{20, 20}, QEvent::MouseButtonRelease);
      CHECK(isScoreCursor(gwin->cursor()));

#if QT_CONFIG(textbrowser)
      // A link in a browser, then off it: the browser restores its own
      // cursor, not the hand.
      const QRect anchorRect = browser->cursorRect(browser->document()->find("a link"));
      const QPoint onLink = browser->viewport()->mapTo(&gallery, anchorRect.center());
      move(gwin, onLink);
      CHECK(isScoreCursor(gwin->cursor()));
      move(gwin, browser->viewport()->mapTo(&gallery, QPoint{5, browser->viewport()->height() - 5}));
      CHECK(browser->viewport()->cursor().shape() != Qt::PointingHandCursor);
      CHECK(isScoreCursor(gwin->cursor()));
#endif

      // The separator between the two docks, the edges of the MDI window.
#if QT_CONFIG(dockwidget)
      const QPoint between
          = (dockA->geometry().bottomLeft() + dockB->geometry().topLeft()) / 2
            + QPoint{dockA->width() / 2, 0};
      move(gwin, between);
      CHECK(isScoreCursor(gwin->cursor()));
#endif
#if QT_CONFIG(mdiarea)
      const QPoint mdiEdge = sub->mapTo(&gallery, QPoint{sub->width() - 1, sub->height() / 2});
      move(gwin, mdiEdge);
      CHECK(isScoreCursor(gwin->cursor()));
#endif
      report("gallery after hovering", audit());
    }

    // A popup menu and the combo box's list.
    {
      QMenu menu;
      menu.addAction("one");
      menu.addAction("two");
      menu.popup(gallery.mapToGlobal(QPoint{100, 100}));
      score::test::settle();
      report("menu", sweep(menu, 6));
      report("menu, after the sweep", audit());
      checkShownBySystem(menu, "menu");
      menu.close();
      combo->showPopup();
      score::test::settle();
      if(auto* popup = combo->view()->window(); popup && popup->isVisible())
      {
        report("combo box list", sweep(*popup, 6));
        checkShownBySystem(*popup, "combo box list");
      }
      combo->hidePopup();
      score::test::settle();
    }

    // Override cursors set by Qt without an event: replaced at the next one.
    {
      // The skin's timer gives an event within 32 ms when nothing else does.
      QGuiApplication::setOverrideCursor(Qt::WaitCursor);
      score::test::run_events_for(50);
      REQUIRE(QGuiApplication::overrideCursor());
      CHECK(QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor);
      QGuiApplication::restoreOverrideCursor();
      CHECK_FALSE(QGuiApplication::overrideCursor());

      line->grabMouse(Qt::CrossCursor);
      move(gwin, centre(*line, gallery));
      REQUIRE(QGuiApplication::overrideCursor());
      CHECK(QGuiApplication::overrideCursor()->shape() == Qt::BitmapCursor);
      line->releaseMouse();
      CHECK_FALSE(QGuiApplication::overrideCursor());
    }

    // Drag and drop: the override cursors of QBasicDrag, over a widget that
    // refuses the drop and over one that takes it.
    if(QGuiApplication::platformName() == QLatin1String("xcb"))
    {
      const QPoint start = centre(*line, gallery);
      const QPoint refusing = centre(*spin, gallery);
      const QPoint taking = centre(*drop, gallery);
      move(gwin, start);
      button(gwin, start, QEvent::MouseButtonPress);

      std::vector<std::pair<std::string, QCursor>> seen;
      auto* drag = new QDrag{line};
      auto* mime = new QMimeData;
      mime->setText("dragged");
      drag->setMimeData(mime);
      bool done = false;
      QTimer::singleShot(50, [&] {
        for(QPoint p : {refusing, taking})
        {
          move(gwin, p, Qt::LeftButton);
          score::test::settle(2);
          move(gwin, p + QPoint{1, 1}, Qt::LeftButton);
          score::test::settle(2);
          if(auto* c = QGuiApplication::overrideCursor())
            seen.emplace_back(p == refusing ? "refusing" : "taking", *c);
        }
        button(gwin, taking + QPoint{1, 1}, QEvent::MouseButtonRelease);
        done = true;
      });
      QTimer::singleShot(5000, [&] {
        if(!done)
          button(gwin, taking, QEvent::MouseButtonRelease);
      });
      drag->exec(Qt::CopyAction | Qt::MoveAction);
      score::test::settle();

      REQUIRE(seen.size() == 2);
      for(auto& [where, c] : seen)
      {
        CAPTURE(where);
        CHECK(c.shape() == Qt::BitmapCursor);
      }
      CHECK(seen[0].second.pixmap().cacheKey()
            == theme.dragCursor(Qt::IgnoreAction).cacheKey());
      // Copy and move offered, no modifier: the target takes a move.
      CHECK(seen[1].second.pixmap().cacheKey()
            == theme.dragCursor(Qt::MoveAction).cacheKey());
      CHECK_FALSE(QGuiApplication::overrideCursor());
    }

#if SCORE_CURSOR_TEST_QUICK
    // A QML scene: QQuickWindow sets its cursors on the QWindow, without
    // going through any widget.
    {
      QQuickWindow quick;
      quick.resize(200, 200);
      auto* item = new QQuickItem{quick.contentItem()};
      item->setSize({100, 100});
      item->setAcceptHoverEvents(true);
      item->setCursor(Qt::IBeamCursor);
      quick.show();
      score::test::settle();
      report("QML window after show", audit());
      move(&quick, {50, 50});
      CHECK(isScoreCursor(quick.cursor()));
      move(&quick, {150, 150});
      CHECK(isScoreCursor(quick.cursor()));
      move(&quick, {50, 50});
      CHECK(isScoreCursor(quick.cursor()));
      quick.hide();
      score::test::settle();
    }
#endif

    // A zoom changed live: every cursor in place is drawn again at the new
    // size, the ones held by graphics items too.
    if(score::canSetGlobalScaleFactorLive()
       && QGuiApplication::platformName() == QLatin1String("xcb"))
    {
      const QSize before = theme.cursor(score::CursorArtwork::IBeam).pixmap().size();
      REQUIRE(score::setGlobalScaleFactor(2.0));
      score::test::settle();
      const QSize after = theme.cursor(score::CursorArtwork::IBeam).pixmap().size();
      CHECK(after == before * 2);
      CHECK(line->cursor().pixmap().size() == after);
      CHECK(skin.CursorPointer.pixmap().size() == QSize(64, 64));
      CHECK(rect->cursor().pixmap().size() == QSize(64, 64));
      report("after the zoom changed", audit());
      report("gallery at 200%", sweep(gallery, 30));
      REQUIRE(score::setGlobalScaleFactor(1.0));
      score::test::settle();
      CHECK(line->cursor().pixmap().size() == before);
    }

    // What the filter costs: an event through the application with it and
    // without it.
    {
      QObject target;
      QEvent ev{QEvent::User};
      constexpr int n = 1'000'000;
      auto time = [&] {
        QElapsedTimer t;
        t.start();
        for(int i = 0; i < n; i++)
          QCoreApplication::sendEvent(&target, &ev);
        return double(t.nsecsElapsed()) / n;
      };
      qApp->removeEventFilter(&flashes);
      const double with = time();
      qApp->removeEventFilter(&theme);
      const double without = time();
      qApp->installEventFilter(&theme);
      std::cerr << "cursor filter: " << with << " ns per event with it, " << without
                << " ns without\n";
      CHECK(with - without < 100.);
      qApp->installEventFilter(&flashes);
    }

    std::cerr << "standard shapes given to a window before score replaced them: "
              << flashes.total() << "\n";
    for(auto& [k, v] : flashes.flashes)
      std::cerr << "  " << k << ": " << v << "\n";

    qApp->removeEventFilter(&flashes);
    gallery.close();
    view->hide();
  });
}
