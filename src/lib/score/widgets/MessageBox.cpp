#include "MessageBox.hpp"
#include <QMessageBox>
#include <score/widgets/Pixmap.hpp>

namespace score
{

int question(
      QWidget* parent,
      const QString& title,
      const QString& text)
{
  auto msg = new QMessageBox{{}, title, text, QMessageBox::Yes | QMessageBox::No, parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_question.png")));

  int idx = msg->exec();
  msg->deleteLater();
  return idx;
}

int information(
      QWidget* parent,
      const QString& title,
      const QString& text)
{
  auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_information.png")));

  int idx = msg->exec();
  msg->deleteLater();
  return idx;
}

int warning(
      QWidget* parent,
      const QString& title,
      const QString& text)
{
  auto msg = new QMessageBox{{}, title, text, QMessageBox::Ok, parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_warning.png")));

  int idx = msg->exec();
  msg->deleteLater();
  return idx;
}
}
