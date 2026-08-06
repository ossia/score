#include "MessageBox.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationSettings.hpp>

#include <QDebug>
#include <QMainWindow>
#include <QMessageBox>

namespace score
{
namespace
{
//! A modal box needs someone to dismiss it, and exec() does not return until
//! one does. `gui` alone does not promise that: an embedder that builds the
//! application without a window -- the test fixtures, anything using
//! MinimalApplication -- leaves it set, and every report of a problem then
//! hangs the process instead of printing. A main window is what actually says
//! there is a person here.
[[maybe_unused]] bool canShowModal() noexcept
{
  return score::AppContext().applicationSettings.gui
         && score::GUIAppContext().mainWindow;
}
}
#if defined(__EMSCRIPTEN__)
namespace
{
// QMessageBox::exec() needs a nested event loop, which the browser main thread
// cannot provide: the call must return before the user answers. Notifications
// (which nothing waits on) are therefore shown modeless, so that errors are at
// least visible; question(), whose answer is a return value, stays text-only.
int notify(
    QWidget* parent, const QString& title, const QString& text, const QString& icon)
{
  qDebug() << title << "\n" << text;

  if(!score::AppContext().applicationSettings.gui)
    return 0;

  auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
  msg->setIconPixmap(score::get_pixmap(icon));
  msg->setAttribute(Qt::WA_DeleteOnClose);
  msg->setWindowModality(Qt::NonModal);
  msg->show();
  msg->raise();
  return 0;
}
}
#endif

int question(QWidget* parent, const QString& title, const QString& text)
{
#if !defined(__EMSCRIPTEN__)
  if(canShowModal())
  {
    auto msg
        = new QMessageBox{{}, title, text, QMessageBox::Yes | QMessageBox::No, parent};
    msg->setIconPixmap(
        score::get_pixmap(QStringLiteral(":/icons/message_question.png")));

    int idx = msg->exec();
    msg->deleteLater();
    return idx;
  }
  else
#endif
  {
    qDebug() << title << "\n" << text;
    return 0;
  }
}

int information(QWidget* parent, const QString& title, const QString& text)
{
#if defined(__EMSCRIPTEN__)
  return notify(
      parent, title, text, QStringLiteral(":/icons/message_information.png"));
#else
  if(canShowModal())
  {
    auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
    msg->setIconPixmap(
        score::get_pixmap(QStringLiteral(":/icons/message_information.png")));

    int idx = msg->exec();
    msg->deleteLater();
    return idx;
  }
  else
  {
    qDebug() << title << "\n" << text;
    return 0;
  }
#endif
}

int warning(QWidget* parent, const QString& title, const QString& text)
{
#if defined(__EMSCRIPTEN__)
  return notify(parent, title, text, QStringLiteral(":/icons/message_warning.png"));
#else
  if(canShowModal())
  {
    auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
    msg->setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_warning.png")));

    int idx = msg->exec();
    msg->deleteLater();
    return idx;
  }
  else
  {
    qDebug() << title << "\n" << text;
    return 0;
  }
#endif
}
}
