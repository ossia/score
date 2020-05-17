#pragma once
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace score
{
// Custom form layout due to
// https://stackoverflow.com/questions/41715124/qformlayout-does-not-expand-widget-full-height-to-maximum-widget-height

class FormLayout : public QGridLayout
{
public:
  void addRow(QString&& text, QWidget* widg)
  {
    const auto nextRow = rowCount();
    addWidget(
        new QLabel{std::move(text)},
        nextRow,
        0,
        1,
        1,
        Qt::AlignHCenter | Qt::AlignTop);
    addWidget(widg, nextRow, 1, 1, 1);
  }
};
}

// REFACTORME - find a better name
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
   // this->setLabelAlignment(Qt::AlignRight);
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
