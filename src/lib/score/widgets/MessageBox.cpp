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
  auto msg = new QMessageBox{parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_question.png")));
  msg->setWindowTitle(title);
  msg->setText(text);

  return msg->exec();
}

int information(
      QWidget* parent,
      const QString& title,
      const QString& text)
{
  auto msg = new QMessageBox{parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_information.png")));
  msg->setWindowTitle(title);
  msg->setText(text);
  msg->addButton(QMessageBox::Ok);

  return msg->exec();
}

int warning(
      QWidget* parent,
      const QString& title,
      const QString& text)
{
  auto msg = new QMessageBox{parent};
  msg->setIconPixmap(score::get_pixmap( QStringLiteral(":/icons/message_warning.png")));
  msg->setWindowTitle(title);
  msg->setText(text);

  return msg->exec();
}
}
