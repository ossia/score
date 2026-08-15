#pragma once

#include <QComboBox>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class ComboBox : public QComboBox
{
public:
  ComboBox(QWidget* parent)
      : QComboBox(parent)
  {
    setDisabled(true);
    connect(this, qOverload<int>(&QComboBox::currentIndexChanged), this, [&](int index) {
      this->setEnabled(index != -1);
    });
  }
};

//! Counterpart of SpinboxWithEnter: reports when the user is done with it.
class SCORE_LIB_BASE_EXPORT ComboBoxWithEnter final : public QComboBox
{
  W_OBJECT(ComboBoxWithEnter)
public:
  explicit ComboBoxWithEnter(QWidget* parent = nullptr);
  ~ComboBoxWithEnter();

  //! Enter, or the focus going elsewhere: take what is in the box.
  void editingFinished() E_SIGNAL(SCORE_LIB_BASE_EXPORT, editingFinished)
  //! Escape: leave the value alone.
  void editingCancelled() E_SIGNAL(SCORE_LIB_BASE_EXPORT, editingCancelled)

private:
  bool event(QEvent* event) override;
};
}
