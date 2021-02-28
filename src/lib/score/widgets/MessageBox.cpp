#include "MessageBox.hpp"

#include <score/widgets/Pixmap.hpp>
#include <score/application/ApplicationContext.hpp>
#include <core/application/ApplicationSettings.hpp>

#include <QMessageBox>
#include <QDebug>

namespace score
{

int question(QWidget* parent, const QString& title, const QString& text)
{
#if !defined(__EMSCRIPTEN__)
  if(score::AppContext().applicationSettings.gui)
  {
    auto msg = new QMessageBox{{}, title, text, QMessageBox::Yes | QMessageBox::No, parent};
    msg->setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_question.png")));

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
#if !defined(__EMSCRIPTEN__)
  if(score::AppContext().applicationSettings.gui)
  {
    auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
    msg->setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_information.png")));

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

int warning(QWidget* parent, const QString& title, const QString& text)
{
#if !defined(__EMSCRIPTEN__)
  if(score::AppContext().applicationSettings.gui)
  {
    auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
    msg->setIconPixmap(score::get_pixmap(QStringLiteral(":/icons/message_warning.png")));

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
}
