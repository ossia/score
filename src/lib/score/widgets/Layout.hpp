#pragma once
#include <QGridLayout>
#include <QLabel>

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
