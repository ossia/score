#pragma once
#include <score/widgets/MarginLess.hpp>
#include <QFormLayout>
namespace Inspector
{
class Layout final : public QFormLayout
{
  public:
    Layout(QWidget* widg = nullptr)
      : QFormLayout{widg}
    {
      this->setContentsMargins(0, 0, 0, 0);
      this->setSpacing(2);
      this->setMargin(2);
      this->setHorizontalSpacing(20);
    }
};
}
