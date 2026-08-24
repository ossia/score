#pragma once
#include <QComboBox>
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
        new QLabel{std::move(text)}, nextRow, 0, 1, 1, Qt::AlignHCenter | Qt::AlignTop);
    addWidget(widg, nextRow, 1, 1, 1);
  }
};
}

// REFACTORME - find a better name
namespace Inspector
{
/**
 * @brief Keeps a widget from forcing the inspector wider than its panel.
 *
 * A combo box sizes its minimum to its widest entry by default: one long
 * device address in a port's combo and every row of the inspector gets
 * pushed past the panel's edge. Sizing it to a few characters instead lets
 * the form give it whatever width the panel has.
 */
inline void fitInInspector(QWidget* w)
{
  if(!w)
    return;

  auto fit = [](QComboBox* cb) {
    cb->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    cb->setMinimumContentsLength(8);
  };

  if(auto cb = qobject_cast<QComboBox*>(w))
    fit(cb);
  for(auto cb : w->findChildren<QComboBox*>())
    fit(cb);
}

class Layout final : public QFormLayout
{
public:
  Layout(QWidget* widg = nullptr)
      : QFormLayout{widg}
  {
    this->setContentsMargins(0, 0, 0, 0);
    this->setSpacing(3);
    this->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    // this->setLabelAlignment(Qt::AlignRight);
  }

  // Every widget going through here is fitted to the panel (see
  // fitInInspector); the QFormLayout overloads are not virtual, so the
  // layout has to be used as an Inspector::Layout for that to apply.
  void addRow(QWidget* label, QWidget* field)
  {
    fitInInspector(field);
    QFormLayout::addRow(label, field);
  }
  void addRow(const QString& label, QWidget* field)
  {
    fitInInspector(field);
    QFormLayout::addRow(label, field);
  }
  void addRow(QWidget* w)
  {
    fitInInspector(w);
    QFormLayout::addRow(w);
  }
  void insertRow(int row, QWidget* label, QWidget* field)
  {
    fitInInspector(field);
    QFormLayout::insertRow(row, label, field);
  }
  void insertRow(int row, const QString& label, QWidget* field)
  {
    fitInInspector(field);
    QFormLayout::insertRow(row, label, field);
  }
  void insertRow(int row, QWidget* w)
  {
    fitInInspector(w);
    QFormLayout::insertRow(row, w);
  }
  using QFormLayout::addRow;
  using QFormLayout::insertRow;
};

class VBoxLayout final : public QVBoxLayout
{
public:
  VBoxLayout(QWidget* widg = nullptr)
      : QVBoxLayout{widg}
  {
    this->setContentsMargins(0, 0, 0, 0);
    this->setSpacing(3);
  }
};
}
