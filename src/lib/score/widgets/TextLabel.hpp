#pragma once
#include <QLabel>

#include <score_lib_base_export.h>
class SCORE_LIB_BASE_EXPORT TextLabel final : public QLabel
{
public:
  TextLabel() { setup(); }

  ~TextLabel() override;

  TextLabel(QWidget* parent) : QLabel(parent) { setup(); }

  TextLabel(const QString& str) : TextLabel() { this->setText(str); }

  TextLabel(const QString& str, QWidget* parent) : QLabel{parent}
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
