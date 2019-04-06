#pragma once
#include <score/widgets/MarginLess.hpp>

#include <QFormLayout>
#include <QVBoxLayout>

namespace Inspector
{
class Layout final : public QFormLayout
{
public:
  Layout(QWidget* widg = nullptr) : QFormLayout{widg}
  {
    this->setContentsMargins(2, 2, 2, 2);
    this->setMargin(0);
    this->setSpacing(3);
    this->setLabelAlignment(Qt::AlignRight);
  }
};

class VBoxLayout final : public QVBoxLayout
{
public:
  VBoxLayout(QWidget* widg = nullptr) : QVBoxLayout{widg}
  {
    this->setContentsMargins(0, 0, 0, 0);
    this->setMargin(0);
    this->setSpacing(3);
  }
};
}
