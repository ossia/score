#pragma once
#include <QLabel>
class TextLabel final : public QLabel
{
public:
  TextLabel()
  {
    setup();
  }

  TextLabel(QWidget* parent):
    QLabel(parent)
  {
    setup();
  }

  TextLabel(const QString& str):
    TextLabel()
  {
    this->setText(str);
  }

  TextLabel(const QString& str, QWidget* parent):
    QLabel{parent}
  {
    setup();
    this->setText(str);
  }

private:
  void setup()
  {
    this->setTextInteractionFlags(Qt::NoTextInteraction);
    this->setTextFormat(Qt::PlainText);
  }
};
