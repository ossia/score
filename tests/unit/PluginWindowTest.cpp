// The window a plug-in's editor lives in goes away the way every plug-in
// host expects it to: through its closeEvent, where each one tells its
// process the editor is gone.

#include <score/widgets/PluginWindow.hpp>

#include <QApplication>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QPointer>

#include <catch2/catch_test_macros.hpp>

namespace
{
struct editor_window final : score::PluginWindow
{
  int* closes{};
  explicit editor_window(int* c)
      : PluginWindow{false, nullptr}
      , closes{c}
  {
  }
  void closeEvent(QCloseEvent* e) override
  {
    ++*closes;
    QDialog::closeEvent(e);
  }
};

int argc = 1;
char arg0[] = "test";
char* argv[] = {arg0, nullptr};
}

TEST_CASE("A plug-in window closed with Escape is told it closes", "[ui][plugins]")
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app{argc, argv};

  int closes = 0;
  QPointer<editor_window> w = new editor_window{&closes};
  w->show();
  QApplication::processEvents();

  QKeyEvent press{QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier};
  QApplication::sendEvent(w, &press);
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QApplication::processEvents();

  // The hosts reset their process's pointer to the window in closeEvent: a
  // window deleted without one leaves that pointer dangling
  CHECK(closes == 1);
  CHECK(w.isNull());
  delete w.data();
}

TEST_CASE("A plug-in window closed by its button is told it closes once", "[ui][plugins]")
{
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QApplication app{argc, argv};

  int closes = 0;
  QPointer<editor_window> w = new editor_window{&closes};
  w->show();
  QApplication::processEvents();
  w->close();
  QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  QApplication::processEvents();

  CHECK(closes == 1);
  CHECK(w.isNull());
  delete w.data();
}
